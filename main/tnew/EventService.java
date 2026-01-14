/*
Senin mevcut EventOut @Getter @Builder ile immutable.
O yüzden out.setGenres(...) gibi bir şey yok. Ben de bu yüzden withGenres() ile yeniden builder yaptım.

*/

import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

import javax.xml.stream.EventFilter;

import org.w3c.dom.events.Event;

import main.transaction.EventDetailsOut;
import main.transaction.EventRepository;
import main.transaction.EventSummaryOut;

public class EventService implements EventApi {

    private final EventRepository eventRepository;
    private final EventMapper eventMapper;
    private final PaginationHelper paginationHelper;
    private final UserServiceInteractor userServiceInteractor;
    private final GenreApi genreApi;
    private final EventGenreMapper eventGenreMapper;

    private static final Logger logger = LoggerFactory.getLogger(EventService.class);
    private static final int SQL_DEFAULT_LIMIT = 30;

    public EventService(
            EventRepository eventRepository,
            EventMapper eventMapper,
            PaginationHelper paginationHelper,
            UserServiceInteractor userServiceInteractor,
            GenreApi genreApi,
            EventGenreMapper eventGenreMapper
    ) {
        this.eventRepository = eventRepository;
        this.eventMapper = eventMapper;
        this.paginationHelper = paginationHelper;
        this.userServiceInteractor = userServiceInteractor;
        this.genreApi = genreApi;
        this.eventGenreMapper = eventGenreMapper;
    }

    // -------- NEW QUERIES --------

    @Override
    public EventOut event(String eventId) {
        return getEvent(eventId);
    }

    @Override
    public EventConnection events(EventScope scope, EventFilter filter, Integer page, Integer size, List<EventSort> sort, String search, String extUserId) {
        validateGenres(filter);

        EventSearchCriteria in = EventSearchMapper.toCriteria(filter, page, size, sort, search);
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(scope == null ? EventScope.PUBLIC : scope)
                .extUserId(extUserId)
                .genres(Optional.ofNullable(in.getGenres()).orElse(List.of()))
                .dateFilter(in.getDateFilter())
                .onlyVisible(in.getOnlyVisible())
                .page(pageOrDefault(in.getPage()))
                .size(sizeOrDefault(in.getSize()))
                .sort(in.getSort())
                .search(in.getSearch())
                .build();

        return getEvents(criteria);
    }

    @Override
    public List<EventOut> activeEvents(EventFilter filter, List<EventSort> sort) {
        validateGenres(filter);

        EventSearchCriteria in = EventSearchMapper.toCriteria(filter, null, null, sort, null);
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(EventScope.PUBLIC)
                .extUserId(null)
                .genres(Optional.ofNullable(in.getGenres()).orElse(List.of()))
                .dateFilter(in.getDateFilter())
                .onlyVisible(in.getOnlyVisible())
                .page(0)
                .size(SQL_DEFAULT_LIMIT)
                .sort(in.getSort())
                .search(in.getSearch())
                .build();

        // flat list
        List<EventSummaryProjection> rows = eventRepository.getActiveEvents(criteria.getOnlyVisible(), criteria.getDateFilter(), criteria.getSort());
        return rows.stream().map(eventMapper::eventSummaryProjectionToEventOut).toList();
    }

    @Override
    public List<EventOut> searchEvents(String keyword) {
        if (keyword == null || keyword.isBlank()
                || keyword.length() < ValidationConstant.USER.SEARCH_MIN_LENGTH
                || keyword.length() > ValidationConstant.USER.SEARCH_MAX_LENGTH) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid keyword length");
        }
        List<EventSummaryProjection> rows = eventRepository.searchEvent(keyword, SQL_DEFAULT_LIMIT);
        return rows.stream().map(eventMapper::eventSummaryProjectionToEventOut).toList();
    }

    // -------- EXISTING (but now return EventOut / EventConnection<EventOut>) --------

    @Override
    public EventConnection getEvents(EventSearchCriteria criteria) {
        List<String> validGenres = genreApi.findValidGenres(Optional.ofNullable(criteria.getGenres()).orElse(List.of()));
        CustomPageable pageable = paginationHelper.getCustomPageable(pageOrDefault(criteria.getPage()), sizeOrDefault(criteria.getSize()));

        EventScope scope = (criteria.getScope() == null) ? EventScope.PUBLIC : criteria.getScope();

        List<EventSummaryProjection> rows;
        int total;

        switch (scope) {
            case PUBLIC -> {
                rows = eventRepository.getAllEvents(validGenres, criteria.getOnlyVisible(), pageable, criteria.getDateFilter(), criteria.getSort());
                total = eventRepository.getAllEventsCount(validGenres, criteria.getDateFilter(), criteria.getOnlyVisible());
            }
            case OWNED -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(criteria.getExtUserId());
                if (user == null || !(user.isOrganizer() || user.isVenue())) {
                    return emptyConnection(criteria);
                }
                RoleEnum role = user.isOrganizer() ? RoleEnum.ORGANIZER : RoleEnum.VENUE;

                rows = eventRepository.getEventsByRole(
                        role, null, user.getOwnerUserId(),
                        criteria.getSearch(), pageable, criteria.getDateFilter(),
                        false, validGenres, criteria.getSort()
                );
                total = eventRepository.getEventsByRoleCount(
                        role, null, user.getOwnerUserId(),
                        criteria.getSearch(), criteria.getDateFilter(),
                        false, validGenres
                );
            }
            case PARTICIPATING -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(criteria.getExtUserId());
                if (user == null) return emptyConnection(criteria);

                RoleEnum role = user.isVenue() ? RoleEnum.VENUE : user.isArtist() ? RoleEnum.ARTIST : null;
                if (role == null) return emptyConnection(criteria);

                rows = eventRepository.getEventsByRole(
                        role, null, user.getOwnerUserId(),
                        criteria.getSearch(), pageable, criteria.getDateFilter(),
                        true, validGenres, criteria.getSort()
                );
                total = eventRepository.getEventsByRoleCount(
                        role, null, user.getOwnerUserId(),
                        criteria.getSearch(), criteria.getDateFilter(),
                        true, validGenres

                );
            }
            default -> { return emptyConnection(criteria); }
        }

        List<EventOut> nodes = rows.stream()
                .map(eventMapper::eventSummaryProjectionToEventOut)
                .toList();

        boolean hasNext = ((pageOrDefault(criteria.getPage()) + 1) * sizeOrDefault(criteria.getSize())) < total;

        return EventConnection.builder()
                .totalCount(total)
                .pageInfo(new PageInfo(pageOrDefault(criteria.getPage()), sizeOrDefault(criteria.getSize()), hasNext))
                .nodes(nodes)
                .build();
    }

    @Override
    public EventOut getEvent(String eventId) {
        Long eventIdLong = parseTsidToLong(eventId);

        Event event = eventRepository.findById(eventIdLong).orElse(null);
        if (event == null || !EventStatus.APPROVED.equals(event.getEventStatus())) {
            throw new NotFoundException("Event is not found for eventId: %s".formatted(eventId));
        }

        EventOut out = eventMapper.eventToEventOut(event);
        return withGenres(event, out);
    }


    @Override
    @Loggable(description = "dashboard event detail for signed-in business user")
    public EventOutDashboard getSignedInUserEventDetail(String extUserId, String eventId) {
        Long eventIdLong = parseTsidToLong(eventId);

        Event event = eventRepository.findById(eventIdLong)
                .orElseThrow(() -> new NotFoundException("Event is not found for eventId: %s".formatted(eventId)));

        UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
        if (user == null || user.getOwnerUserId() == null) {
            throw new ForbiddenException("This user is not authorized to see event details.");
        }

        List<UUID> ownerIds = event.getOwnerUserIds();
        if (ownerIds == null || !ownerIds.contains(user.getOwnerUserId())) {
            throw new ForbiddenException("This user is not authorized to see event details.");
        }

        // Mapper sadece event microservice’in sorumlu olduğu alanları doldurmalı:
        // - id alanları (ownerUserIds, venueUserId, addressId, artistIds)
        // - eventStatus, isVisible, name, dates, imageUrl, maxImagePixel, otherArtists
        // organizers/artists/venue/address/tickets -> federation resolver ile dolacak
        return eventMapper.eventToEventOutDashboard(event, eventGenreMapper.entityToDto(event.getEventGenre()));
    }

    @Override
    public List<EventOut> eventInfoList(List<String> eventIds) {
        if (eventIds == null || eventIds.isEmpty()) return List.of();

        List<Long> ids = eventIds.stream()
                .filter(s -> s != null && !s.isBlank())
                .map(this::parseTsidToLong)   // TSID string -> long
                .toList();

        if (ids.isEmpty()) return List.of();

        List<Event> events = eventRepository.findAllById(ids);

        return events.stream()
                .map(e -> EventOut.builder()
                        .eventId(Tsid.from(e.getEventId()).toString())
                        .name(e.getName())
                        .startDate(e.getStartDate())
                        .endDate(e.getEndDate())
                        .imageUrl(e.getImageId() == null ? null : storageHelper.getImageUrl(e.getImageId()))
                        .maxImagePixel(e.getMaxImagePixel())
                        .ownerUserIds(e.getOwnerUserIds() == null ? List.of() : e.getOwnerUserIds().stream().map(UUID::toString).toList())
                        .venueUserId(e.getVenueId() == null ? null : e.getVenueId().toString())
                        .addressId(e.getAddressId() == null ? null : e.getAddressId().toString())
                        .artistIds(e.getArtists() == null ? List.of() : e.getArtists().stream().map(UUID::toString).toList())
                        .isVisible(e.getIsVisible())
                        .eventStatus(e.getEventStatus())
                        .build()
                )
                .toList();
    }
    
    @Override
    public List<String> getBusinessUserEventIds(UUID userId) {
        return eventRepository.findEventIdsByOwnerAndStatus(userId, EventStatus.APPROVED)
                .stream().map(id -> Tsid.from(id).toString()).toList();
    }

    @Override
    public EventDetailsOut getEventDetails(String eventId, String extUserId) {
        Long id = parseTsidToLong(eventId);

        Event event = eventRepository.findById(id)
                .orElseThrow(() -> new NotFoundException("Event not found for eventId: %s".formatted(eventId)));

        UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
        if (user == null || user.getOwnerUserId() == null) throw new ForbiddenException("Invalid user");

        Boolean editable = eventRepository.isEventEditable(id, user.getOwnerUserId());

        return EventDetailsOut.builder()
                .eventId(eventId)
                .startDate(event.getStartDate())
                .endDate(event.getEndDate())
                .ownerBusinessUserId(user.getOwnerUserId().toString())
                .isEventOwner(editable)
                .build();
    }

    @Override
    public List<EventOut> search(String searchTerm) {
        // keep your existing method if other services rely on it,
        // but better: return List<EventOut> directly in new searchEvents
        List<EventOut> rows = eventRepository.searchEvent(searchTerm, SQL_DEFAULT_LIMIT);
        return rows.stream().map(p -> {
            // if you still have old EventSummaryOut used elsewhere:
            return null;
        }).toList();
    }

    // -------- Helpers --------
    private EventOut withGenres(Event event, EventOut out) {
        List<String> genres = eventGenreMapper.entityToDto(event.getEventGenre());

        return EventOut.builder()
                .eventId(out.getEventId())
                .name(out.getName())
                .description(out.getDescription())
                .startDate(out.getStartDate())
                .endDate(out.getEndDate())
                .imageUrl(out.getImageUrl())
                .maxImagePixel(out.getMaxImagePixel())
                .isVisible(out.getIsVisible())
                .eventStatus(out.getEventStatus())
                .ownerUserIds(out.getOwnerUserIds())
                .venueUserId(out.getVenueUserId())
                .addressId(out.getAddressId())
                .artistIds(out.getArtistIds())
                .locality(out.getLocality())
                .administrativeArea(out.getAdministrativeArea())
                .country(out.getCountry())
                .genres(genres)
                .otherArtists(out.getOtherArtists())
                .tickets(out.getTickets())
                .hasTicket(out.getHasTicket())
                .organizers(out.getOrganizers())
                .artists(out.getArtists())
                .venue(out.getVenue())
                .address(out.getAddress())
                .build();
    }

    private EventConnection emptyConnection(EventSearchCriteria c) {
        return EventConnection.builder()
                .totalCount(0)
                .pageInfo(new PageInfo(pageOrDefault(c.getPage()), sizeOrDefault(c.getSize()), false))
                .nodes(List.of())
                .build();
    }

    private void validateGenres(EventFilter filter) {
        if (filter != null && filter.getGenres() != null && filter.getGenres().size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }
    }

    private int pageOrDefault(Integer page) { return page == null || page < 0 ? 0 : page; }
    private int sizeOrDefault(Integer size) { return size == null || size <= 0 ? 12 : size; }

    private Long parseTsidToLong(String tsidStr) {
        try { return Tsid.from(tsidStr).toLong(); }
        catch (Exception e) { throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid event ID format"); }
    }

    private EventOut summaryToEventOutShallow(EventSummaryOut s) {
        // if you remove EventSummaryOut completely, delete this helper
        return EventOut.builder()
                .eventId(s.getEventId())
                .name(s.getName())
                .startDate(s.getStartDate())
                .endDate(s.getEndDate())
                .imageUrl(s.getImageUrl())
                .maxImagePixel(s.getMaxImagePixel())
                .ownerUserIds(/* parse if needed */ List.of())
                .venueUserId(s.getVenueUserId())
                .addressId(s.getAddressId())
                .locality(s.getLocality())
                .administrativeArea(s.getAdministrativeArea())
                .country(s.getCountry())
                .hasTicket(s.getHasTicket())
                .isVisible(s.getIsVisible())
                .eventStatus(s.getEventStatus())
                .organizers(s.getOrganizers())
                .build();
    }



    @Override
    @Loggable(description = "user events by username")
    public UserEventOut userEventsByUsername(String username, int page, int size, DateFilterEnum dateFilterEnum) {

        UserVerificationDto userVerification = userServiceInteractor.verifyBusinessUserByName(username);
        if (userVerification == null) {
            return UserEventOut.builder()
                    .username(username)
                    .events(List.of())
                    .build();
        }

        List<EventSummaryProjection> rows = findEventSummaryProjections(
                EventStatus.APPROVED,
                null,
                page,
                size,
                userVerification,
                false,
                dateFilterEnum
        );

        List<Event> events = (rows == null ? List.<Event>of() : rows.stream()
                .map(eventMapper::eventSummaryProjectionToEvent)  // <-- yeni mapper
                .toList());

        // displayName/userImageUrl vs user ms’den geliyor demiştin: burada doldurmak istemezsen boş bırak
        // ama eski UserEventOut bunu dolduruyordu. Bu bilgi userServiceInteractor’dan alınabiliyorsa doldur.
        return UserEventOut.builder()
                .username(username)
                .displayName(userVerification.getDisplayName())     // varsa
                .userImageUrl(userVerification.getImageUrl())       // varsa
                .maxImagePixel(userVerification.getMaxImagePixel()) // varsa
                .events(events)
                .build();
    }

    @Override
    @Loggable(description = "event analysis list")
    public List<EventAnalysisOut> eventAnalysisList(Instant from, Instant to, List<String> genres, String extUserId) {

        List<String> validGenres = genreApi.findValidGenres(Optional.ofNullable(genres).orElse(List.of()));

        UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
        if (user == null || !(user.isOrganizer() || user.isVenue())) {
            return List.of();
        }

        RoleEnum role = user.isOrganizer() ? RoleEnum.ORGANIZER : RoleEnum.VENUE;

        List<EventSummaryProjection> rows = eventRepository.getEventsByRole(
                role,
                null,
                user.getOwnerUserId(),
                null,
                null,
                null,
                false,
                validGenres,
                null
        );

        return rows.stream()
                .map(eventMapper::eventSummaryProjectionToEventAnalysisOut)
                .peek(out -> {
                    out.setFilterFrom(from);
                    out.setFilterTo(to);
                })
                .toList();
    }

    @Override
    @Loggable(description = "upcoming events with ticket info")
    public List<EventWithTicketOut> getUpcomingEventsWithTicketInfo(int limit) {

        int safeLimit = (limit <= 0 ? 30 : Math.min(limit, 50));

        List<EventSort> sort = List.of(new EventSort(EventSortField.START_DATE, SortDirection.ASC));
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(EventScope.PUBLIC)
                .extUserId(null)
                .genres(List.of())
                .dateFilter(DateFilterEnum.UPCOMING)
                .onlyVisible(true)
                .page(0)
                .size(safeLimit)
                .sort(sort)
                .search(null)
                .build();

        EventConnection conn = getEvents(criteria);

        return Optional.ofNullable(conn.getNodes()).orElse(List.of())
                .stream()
                .map(eventMapper::eventToEventWithTicketOut) // <-- yeni mapper (Event -> EventWithTicketOut)
                .toList();
    }

    @Override
    @Loggable(description = "news related events with ticket info")
    public List<EventWithTicketOut> getNewsRelatedEventsWithTicketInfo(String newsId, int limit) {

        int safeLimit = (limit <= 0 ? 30 : Math.min(limit, 50));

        List<Long> relatedEventIds = newsApi.getRelatedEventIds(newsId);
        if (relatedEventIds == null || relatedEventIds.isEmpty()) return List.of();

        // repo zaten projections döndürüyordu
        List<EventSummaryOut> summaryOuts = getNewsRelatedEventsWithTicketInfo(relatedEventIds); 
        // ^ sende zaten vardı: eventIds ile active events alıp EventSummaryOut mapliyordu
        // Ama artık "Event" hedefimiz var, onun için o metodu da Event döndürecek hale getirmen daha iyi.

        // Daha temiz: repo’dan projections çek -> Event -> EventWithTicketOut
        List<EventSort> sort = List.of(new EventSort(EventSortField.START_DATE, SortDirection.ASC));
        List<EventSummaryProjection> rows = eventRepository.getActiveEventsByIds(
                relatedEventIds,
                true,
                DateFilterEnum.UPCOMING,
                sort,
                safeLimit
        );

        List<Event> events = rows.stream().map(eventMapper::eventSummaryProjectionToEvent).toList();
        return events.stream().map(eventMapper::eventToEventWithTicketOut).toList();
    }



}
