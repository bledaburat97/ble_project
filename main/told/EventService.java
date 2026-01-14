@Service
public class EventService implements EventApi {
    private final EventRepository eventRepository;
    private final TsidFactory eventIdTsidFactory;
    private final EventStatusHistoryRepository eventStatusHistoryRepository;
    private final EventMapper eventMapper;
    private final PaginationHelper paginationHelper;
    private final UserServiceInteractor userServiceInteractor;
    private final TransactionTemplate transactionTemplate;
    private final TicketServiceInteractor ticketServiceInteractor;
    private final EventGenreMapper eventGenreMapper;
    private final GenreApi genreApi;
    private final StorageHelper storageHelper;
    private final ReservationServiceInteractor reservationServiceInteractor;

    private static final Logger logger = LoggerFactory.getLogger(EventService.class);


    public EventService(EventRepository eventRepository, TsidFactory eventIdTsidFactory, EventStatusHistoryRepository eventStatusHistoryRepository,
                        EventMapper eventMapper, PaginationHelper paginationHelper, UserServiceInteractor userServiceInteractor, TransactionTemplate transactionTemplate,
                        TicketServiceInteractor ticketServiceInteractor, EventGenreMapper eventGenreMapper,
                        GenreApi genreApi, StorageHelper storageHelper, ReservationServiceInteractor reservationServiceInteractor) {
        this.eventRepository = eventRepository;
        this.eventIdTsidFactory = eventIdTsidFactory;
        this.eventStatusHistoryRepository = eventStatusHistoryRepository;
        this.eventMapper = eventMapper;
        this.paginationHelper = paginationHelper;
        this.userServiceInteractor = userServiceInteractor;
        this.transactionTemplate = transactionTemplate;
        this.ticketServiceInteractor = ticketServiceInteractor;
        this.eventGenreMapper = eventGenreMapper;
        this.genreApi = genreApi;
        this.storageHelper = storageHelper;
        this.reservationServiceInteractor = reservationServiceInteractor;
    }

    @Override
    public List<EventSummaryOut> getNewsRelatedEventsWithTicketInfo(List<Long> eventIds) {

        List<EventSort> sort = List.of(new EventSort(EventSortField.START_DATE, SortDirection.ASC));

        List<EventSummaryProjection> rows = eventRepository.getActiveEventsByIds(
                eventIds,
                true,
                DateFilterEnum.UPCOMING,
                sort,
                30
        );

        return rows.stream()
                .map(eventMapper::eventSummaryProjectionToEventSummaryOut)
                .toList();
    }

    @Override
    @Loggable(description = "get events")
    public List<EventAnalysisOut> getEventAnalysisList(Instant from,  Instant to, List<String> genres, String extUserId) {

        List<String> validGenres = genreApi.findValidGenres(
                Optional.ofNullable(genres).orElse(List.of())
        );

        UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
        if (user == null || !(user.isOrganizer() || user.isVenue())) {
            return List.of();
        }
        UUID ownerUserId = user.getOwnerUserId();

        RoleEnum role = user.isOrganizer() ? RoleEnum.ORGANIZER
                : user.isVenue() ? RoleEnum.VENUE
                : null;
        if (role == null) return List.of();

        List<EventSummaryProjection> eventSummaryProjections = eventRepository.getEventsByRole(
                role,
                null,
                ownerUserId,
                null,
                null,
                null,
                false,
                validGenres,
                null
        );

        return eventSummaryProjections.stream()
                .map(eventMapper::eventSummaryProjectionToEventAnalysisOut)
                .map(out -> {
                    out.setFilterFrom(from);
                    out.setFilterTo(to);
                    return out;
                })
                .toList();
    }

    @Override
    @Loggable(description = "get events")
    public EventConnection getEvents(EventSearchCriteria criteria) {

        List<String> validGenres = genreApi.findValidGenres(
                Optional.ofNullable(criteria.getGenres()).orElse(List.of())
        );

        CustomPageable pageable = paginationHelper.getCustomPageable(criteria.getPage(), criteria.getSize());

        List<EventSummaryProjection> rows;
        int total;

        EventScope scope = (criteria.getScope() == null) ? EventScope.PUBLIC : criteria.getScope();

        switch (scope) {
            case PUBLIC -> {
                rows  = eventRepository.getAllEvents(validGenres, criteria.getOnlyVisible(), pageable, criteria.getDateFilter(), criteria.getSort());
                total = eventRepository.getAllEventsCount(validGenres, criteria.getDateFilter(), criteria.getOnlyVisible());
            }
            case OWNED -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(criteria.getExtUserId());
                if (user == null || !(user.isOrganizer() || user.isVenue())) {
                    return emptyConnection(criteria);
                }
                UUID ownerUserId = user.getOwnerUserId();

                RoleEnum role = user.isOrganizer() ? RoleEnum.ORGANIZER
                        : user.isVenue() ? RoleEnum.VENUE
                        : null;
                if (role == null) return emptyConnection(criteria);

                rows  = eventRepository.getEventsByRole(
                        role,
                        null,
                        ownerUserId,
                        criteria.getSearch(),
                        pageable,
                        criteria.getDateFilter(),
                        false,                // getOnlyVenueIsNotOwnerEvents
                        validGenres,
                        criteria.getSort()
                );
                total = eventRepository.getEventsByRoleCount(
                        role,
                        null,
                        ownerUserId,
                        criteria.getSearch(),
                        criteria.getDateFilter(),
                        false,
                        validGenres
                );
            }
            case PARTICIPATING -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(criteria.getExtUserId());
                if (user == null) return emptyConnection(criteria);

                RoleEnum role = user.isVenue() ? RoleEnum.VENUE
                        : user.isArtist() ? RoleEnum.ARTIST
                        : null;
                if (role == null) return emptyConnection(criteria);

                rows  = eventRepository.getEventsByRole(
                        role,
                        null,
                        user.getOwnerUserId(),
                        criteria.getSearch(),
                        pageable,
                        criteria.getDateFilter(),
                        true,
                        validGenres,
                        criteria.getSort()
                );
                total = eventRepository.getEventsByRoleCount(
                        role,
                        null,
                        user.getOwnerUserId(),
                        criteria.getSearch(),
                        criteria.getDateFilter(),
                        true,
                        validGenres
                );
            }
            default -> {
                return emptyConnection(criteria);
            }
        }

        List<EventSummaryOut> nodes = rows.stream()
                .map(eventMapper::eventSummaryProjectionToEventSummaryOut)
                .toList();

        boolean hasNext = ((criteria.getPage() + 1) * criteria.getSize()) < total;

        return EventConnection.builder()
                .totalCount(total)
                .pageInfo(new PageInfo(criteria.getPage(), criteria.getSize(), hasNext))
                .nodes(nodes)
                .build();
    }

    @Override
    @Loggable(description = "list active public events (flat)")
    public List<ActiveEventInfo> getActiveEvents(EventSearchCriteria c) {
        List<EventSummaryProjection> rows = eventRepository.getActiveEvents(
                c.getOnlyVisible(),
                c.getDateFilter(),
                c.getSort()
        );

        return rows.stream()
                .map(eventMapper::eventSummaryProjectionToActiveEventInfo)
                .toList();
    }

    private EventConnection emptyConnection(EventSearchCriteria c) {
        return EventConnection.builder()
                .totalCount(0)
                .pageInfo(new PageInfo(c.getPage(), c.getSize(), false))
                .nodes(List.of())
                .build();
    }


    @Override
    @Loggable(description = "to get event info by id")
    public EventSummaryOut getEventInfo(String eventId) {
        Long longEventId = Long.parseLong(eventId);

        Optional<Event> eventOptional = eventRepository.findById(longEventId);

        return eventOptional
                .map(eventMapper::eventToEventSummaryOut)
                .orElseThrow(() -> new RuntimeException("Event not found with ID: " + eventId));
    }

    @Loggable(description = "to get specific event")
    public EventOut getEvent(String eventId) {
        Event event;
        try {
            Long eventIdLong = Tsid.from(eventId).toLong();
            event = eventRepository.findById(eventIdLong).orElse(null);
        } catch (IllegalArgumentException e) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid event ID format");
        }

        if (event == null || !EventStatus.APPROVED.equals(event.getEventStatus())) {
            throw new NotFoundException("Event is not found for eventId: %s".formatted(eventId));
        }

        String eventIdStr = Tsid.from(event.getEventId()).toString();

        String ownerUserIds = event.getOwnerUserIds().stream().map(UUID::toString).collect(Collectors.joining(","));
        EventOut eventOut = EventOut.builder()
            .eventId(eventIdStr)
            .name(event.getName())
            .description(event.getDescription())
            .otherArtists(event.getNonUserArtist())
            .startDate(event.getStartDate())
            .endDate(event.getEndDate())
            .genres(eventGenreMapper.entityToDto(event.getEventGenre()))
            .imageUrl(storageHelper.getImageUrl(event.getImageId()))
            .ownerUserIds(ownerUserIds)
            .venueUserId(event.getVenueId() == null ? null : event.getVenueId().toString())
            .artistIds(event.getArtists() == null ? null : event.getArtists().stream().map(UUID::toString).collect(Collectors.joining(",")))
            .addressId(event.getAddressId() == null ? null : event.getAddressId().toString())
            .maxImagePixel(event.getMaxImagePixel())
            .build();

        return eventOut;
    }

    private List<EventSummaryProjection> findEventSummaryProjections(EventStatus desiredStatus, String keyword, int page, int size, UserVerificationDto userVerification, boolean getOnlyVenueIsNotOwnerEvents, DateFilterEnum dateFilterEnum) {
        List<EventSummaryProjection> events = null;

        List<String> validGenres = genreApi.findValidGenres(List.of());

        if (userVerification.isOrganizer()) {
            events = eventRepository.getEventsByRole(RoleEnum.ORGANIZER, desiredStatus, userVerification.getOwnerUserId(), keyword, new CustomPageable(page, size), dateFilterEnum, false, validGenres,  null);
        } else if (userVerification.isVenue()) {
            events = eventRepository.getEventsByRole(RoleEnum.VENUE, desiredStatus, userVerification.getOwnerUserId(), keyword, new CustomPageable(page, size), dateFilterEnum, getOnlyVenueIsNotOwnerEvents, validGenres, null);
        } else if (userVerification.isArtist()) {
            events = eventRepository.getEventsByRole(RoleEnum.ARTIST, desiredStatus, userVerification.getOwnerUserId(), keyword, new CustomPageable(page, size), dateFilterEnum, false, validGenres, null);
        }
        return events;
    }

    @Override
    @Loggable(description = "to search user events")
    public UserEventOut searchUserEventsByUsername(int page, int size, String username, DateFilterEnum dateFilterEnum) {
        UserVerificationDto userVerification = userServiceInteractor.verifyBusinessUserByName(username);
        List<EventSummaryProjection> events = findEventSummaryProjections(EventStatus.APPROVED, null, page, size, userVerification, false, dateFilterEnum);
        List<EventSummaryOut> outs = events == null ? List.of() : events.stream().map(eventMapper::eventSummaryProjectionToEventSummaryOut).toList();
        return UserEventOut.builder().username(username).events(outs).build();
    }

    @Override
    @Loggable(description = "to get specific event for signed in user")
    public EventOutDashboard getSignedInUserEventDetail(String extUserId, String eventId) {
        Long eventIdLong = Tsid.from(eventId).toLong();
        Event event = eventRepository.findById(eventIdLong).orElse(null);

        if (event == null) {
            throw new NotFoundException("Event is not found for eventId: %s".formatted(eventId));
        }

        UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
        List<UUID> eventOwnerUserIds = event.getOwnerUserIds();
        if(user == null || user.getOwnerUserId() == null || !eventOwnerUserIds.contains(user.getOwnerUserId())) {
            throw new ForbiddenException("This user is not authorized to see event details.");
        }

        String eventIdStr = Tsid.from(event.getEventId()).toString();
        String ownerUserIds = eventOwnerUserIds.stream().map(UUID::toString).collect(Collectors.joining(","));

        return EventOutDashboard.builder()
            .eventId(eventIdStr)
            .name(event.getName())
            .description(event.getDescription())
            .otherArtists(event.getNonUserArtist())
            .startDate(event.getStartDate())
            .endDate(event.getEndDate())
            .genres(eventGenreMapper.entityToDto(event.getEventGenre()))
            .imageUrl(storageHelper.getImageUrl(event.getImageId()))
            .eventStatus(event.getEventStatus())
            .ownerUserIds(ownerUserIds)
            .venueUserId(event.getVenueId() == null ? null : event.getVenueId().toString())
            .artistIds(event.getArtists() == null ? null : event.getArtists().stream().map(UUID::toString).collect(Collectors.joining(",")))
            .addressId(event.getAddressId() == null ? null : event.getAddressId().toString())
            .maxImagePixel(event.getMaxImagePixel())
            .isVisible(event.getIsVisible())
            .build();
    }

    public EventDetailsOut getEventDetails(String eventId, String extUserId) {
        Long eventIdLong = Tsid.from(eventId).toLong();
        Event event = eventRepository.findById(eventIdLong).orElseThrow(() ->
                new NotFoundException("Event not found for eventId: %s".formatted(eventId))
        );

        UserVerificationDto userVerification = userServiceInteractor.verifyBusinessUserById(extUserId);

        Boolean isEventOwner = eventRepository.isEventEditable(eventIdLong, userVerification.getOwnerUserId());


        return EventDetailsOut.builder()
                .eventId(eventId)
                .startDate(event.getStartDate())
                .endDate(event.getEndDate())
                .ownerBusinessUserId(userVerification.getOwnerUserId().toString())
                .isEventOwner(isEventOwner)
                .build();
    }


    @Override
    @Loggable(description = "to get event info list")
    public List<EventInfo> getEventInfoList(List<String> eventIds) {
        List<Long> longEventIds = eventIds.stream()
                .map(Long::parseLong)
                .collect(Collectors.toList());

        List<Event> events = eventRepository.findAllById(longEventIds);

        return events.stream()
                .map(event -> new EventInfo(
                        event.getEventId().toString(),
                        event.getName(),
                        event.getStartDate(),
                        event.getEndDate(),
                        event.getImageId() != null ? storageHelper.getImageUrl(event.getImageId()) : null
                ))
                .collect(Collectors.toList());
    }

    public List<String> getBusinessUserEventIds(UUID userId) {
        List<Long> eventIds = eventRepository.findEventIdsByOwnerAndStatus(
                userId,
                EventStatus.APPROVED
        );

        // eventIds boşsa zaten .stream().toList() yine boş döner; ekstra check şart değil ama istersen bırakabilirsin.
        return eventIds.stream()
                .map(id -> Tsid.from(id).toString())
                .toList();
    }

    @Override
    @Loggable(description = "to search with given search values")
    public List<EventSummaryOut> search(String searchTerm) {
        List<EventSummaryProjection> eventSummaryProjections = eventRepository.searchEvent(searchTerm, SQL_DEFAULT_LIMIT);
        return eventSummaryProjections.stream().map(eventMapper::eventSummaryProjectionToEventSummaryOut).toList();
    }

}