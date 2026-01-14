@Service
@RequiredArgsConstructor
public class EventServiceImpl implements EventService {

    private static final int DEFAULT_SIZE = 12;

    private final EventRepository eventRepository;
    private final EventMapper eventMapper;
    private final GenreApi genreApi;
    private final PaginationHelper paginationHelper;
    private final UserServiceInteractor userServiceInteractor;
    private final StorageHelper storageHelper;
    private final EventGenreMapper eventGenreMapper; // varsa

    @Override
    public EventOut getEventById(String eventId) {
        Long eventIdLong = parseTsidToLong(eventId);

        Event event = eventRepository.findById(eventIdLong).orElse(null);
        if (event == null || !EventStatus.APPROVED.equals(event.getEventStatus())) {
            throw new NotFoundException("Event is not found for eventId: %s".formatted(eventId));
        }

        // full mapping
        EventOut out = eventMapper.eventToEventOut(event);

        // genres kesin gerekiyorsa burada doldur (mapper içinde de yapılabilir)
        if (event.getEventGenre() != null) {
            out.setGenres(eventGenreMapper.entityToDto(event.getEventGenre()));
        }

        return out;
    }

    @Override
    public EventConnection getEvents(
            EventScope scope,
            EventFilter filter,
            Integer page,
            Integer size,
            List<EventSort> sort,
            String search,
            String extUserId
    ) {
        validateGenres(filter);

        // scope default
        EventScope resolvedScope = (scope == null) ? EventScope.PUBLIC : scope;

        // search validasyonu: yalnızca doluysa kontrol et
        validateSearchIfPresent(search);

        // genres -> validGenres (DB’de gerçekten olanlar)
        List<String> requestedGenres = (filter == null || filter.getGenres() == null) ? List.of() : filter.getGenres();
        List<String> validGenres = genreApi.findValidGenres(requestedGenres);

        Boolean onlyVisible = (filter == null) ? null : filter.getOnlyVisible();
        DateFilterEnum dateFilter = (filter == null) ? null : filter.getDateFilter();

        int p = pageOrDefault(page);
        int s = sizeOrDefault(size);

        CustomPageable pageable = paginationHelper.getCustomPageable(p, s);

        List<EventSummaryProjection> rows;
        int total;

        switch (resolvedScope) {

            case PUBLIC -> {
                // ✅ ÖNEMLİ: public scope search repo’ya gidecek
                rows = eventRepository.getAllEvents(
                        validGenres,
                        onlyVisible,
                        pageable,
                        dateFilter,
                        sort,
                        search
                );

                total = eventRepository.getAllEventsCount(
                        validGenres,
                        dateFilter,
                        onlyVisible,
                        search
                );
            }

            case OWNED -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
                if (user == null || !(user.isOrganizer() || user.isVenue())) {
                    return emptyConnection(p, s);
                }

                RoleEnum role = user.isOrganizer() ? RoleEnum.ORGANIZER : RoleEnum.VENUE;

                rows = eventRepository.getEventsByRole(
                        role,
                        null,
                        user.getOwnerUserId(),
                        search,
                        pageable,
                        dateFilter,
                        false,
                        validGenres,
                        sort
                );

                total = eventRepository.getEventsByRoleCount(
                        role,
                        null,
                        user.getOwnerUserId(),
                        search,
                        dateFilter,
                        false,
                        validGenres
                );
            }

            case PARTICIPATING -> {
                UserVerificationDto user = userServiceInteractor.verifyBusinessUserById(extUserId);
                if (user == null) return emptyConnection(p, s);

                RoleEnum role = user.isVenue() ? RoleEnum.VENUE : (user.isArtist() ? RoleEnum.ARTIST : null);
                if (role == null) return emptyConnection(p, s);

                rows = eventRepository.getEventsByRole(
                        role,
                        null,
                        user.getOwnerUserId(),
                        search,
                        pageable,
                        dateFilter,
                        true,
                        validGenres,
                        sort
                );

                total = eventRepository.getEventsByRoleCount(
                        role,
                        null,
                        user.getOwnerUserId(),
                        search,
                        dateFilter,
                        true,
                        validGenres
                );
            }

            default -> {
                return emptyConnection(p, s);
            }
        }

        List<EventOut> nodes = rows.stream()
                .map(eventMapper::eventSummaryProjectionToEventOut) // lightweight
                .toList();

        boolean hasNext = ((p + 1) * s) < total;

        return EventConnection.builder()
                .totalCount(total)
                .pageInfo(new PageInfo(p, s, hasNext))
                .nodes(nodes)
                .build();
    }

    @Override
    public List<EventOut> getEventsByIds(List<String> eventIds) {
        if (eventIds == null || eventIds.isEmpty()) return List.of();

        // parse TSID -> long (invalid olanlar tüm listeyi patlatmasın istiyorsan try/catch ile skip edebilirsin)
        List<Long> ids = eventIds.stream()
                .filter(s -> s != null && !s.isBlank())
                .map(this::parseTsidToLong)
                .toList();

        if (ids.isEmpty()) return List.of();

        List<Event> events = eventRepository.findAllById(ids);

        // input sırası korunmalı
        Map<Long, Event> byId = events.stream()
                .collect(Collectors.toMap(Event::getEventId, Function.identity(), (a, b) -> a));

        List<EventOut> out = new ArrayList<>(ids.size());
        for (Long id : ids) {
            Event e = byId.get(id);
            if (e == null) continue;

            out.add(EventOut.builder()
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
                    .build());
        }
        return out;
    }

    // ---------------- Helpers ----------------

    private EventConnection emptyConnection(int page, int size) {
        return EventConnection.builder()
                .totalCount(0)
                .pageInfo(new PageInfo(page, size, false))
                .nodes(List.of())
                .build();
    }

    private void validateGenres(EventFilter filter) {
        if (filter != null && filter.getGenres() != null
                && filter.getGenres().size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }
    }

    private void validateSearchIfPresent(String search) {
        if (search == null || search.isBlank()) return;

        int len = search.trim().length();
        if (len < ValidationConstant.USER.SEARCH_MIN_LENGTH
                || len > ValidationConstant.USER.SEARCH_MAX_LENGTH) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid keyword length");
        }
    }

    private int pageOrDefault(Integer page) { return (page == null || page < 0) ? 0 : page; }
    private int sizeOrDefault(Integer size) { return (size == null || size <= 0) ? DEFAULT_SIZE : size; }

    private Long parseTsidToLong(String tsidStr) {
        try { return Tsid.from(tsidStr).toLong(); }
        catch (Exception e) { throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid event ID format"); }
    }
}