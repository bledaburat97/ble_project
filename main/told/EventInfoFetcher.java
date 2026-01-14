@DgsComponent
public class EventInfoFetcher {

    private final EventApi eventApi;
    private final NewsApi newsApi;
    private UserHelper userHelper;

    private static final Logger logger = LoggerFactory.getLogger(EventDataFetcher.class);
    private static final int MAX_UPCOMING_EVENT_SIZE = 30;

    public EventInfoFetcher(EventApi eventApi, NewsApi newsApi, UserHelper userHelper) {
        this.eventApi = eventApi;
        this.newsApi = newsApi;
        this.userHelper = userHelper;
    }

    @DgsQuery
    public EventConnection getPublicEvents(
            @InputArgument EventFilter filter,
            @InputArgument Integer page,
            @InputArgument Integer size,
            @InputArgument List<EventSort> sort
    ) {
        if (filter != null && filter.getGenres() != null &&
                filter.getGenres().size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }

        logger.error("getPublicEvents: filter: {}, page: {}, size: {}", filter, page, size);

        EventSearchCriteria in = EventSearchMapper.toCriteria(filter, page, size, sort, null);
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(EventScope.PUBLIC)
                .extUserId(null)
                .genres(in.getGenres())
                .dateFilter(in.getDateFilter())
                .onlyVisible(in.getOnlyVisible())
                .page(in.getPage())
                .size(in.getSize())
                .sort(in.getSort())
                .search(in.getSearch())
                .build();

        return eventApi.getEvents(criteria);
    }

    @PreAuthorize("(#scope.name() == 'OWNED' and hasAnyRole('VENUE','ORGANIZER')) " + "or (#scope.name() == 'PARTICIPATING' and hasAnyRole('VENUE','ARTIST'))")
    @DgsQuery
    public EventConnection getUserEvents(
            @InputArgument EventScope scope,
            @InputArgument EventFilter filter,
            @InputArgument Integer page,
            @InputArgument Integer size,
            @InputArgument List<EventSort> sort,
            @InputArgument String search
    ) {
        if (filter != null && filter.getGenres() != null &&
                filter.getGenres().size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }

        logger.error("getUserEvents: scope: {}, filter: {}, page: {}, size: {}", scope, filter, page, size);
        String extUserId = null;
        if (scope != null && scope != EventScope.PUBLIC) {
            extUserId = getExtUserId();
        }

        EventSearchCriteria in = EventSearchMapper.toCriteria(filter, page, size, sort, search);
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(scope == null ? EventScope.PUBLIC : scope)
                .extUserId(extUserId)
                .genres(in.getGenres())
                .dateFilter(in.getDateFilter())
                .onlyVisible(in.getOnlyVisible())
                .page(in.getPage())
                .size(in.getSize())
                .sort(in.getSort())
                .search(in.getSearch())
                .build();

        return eventApi.getEvents(criteria);
    }



    @PreAuthorize("hasAnyRole('VENUE','ORGANIZER')")
    @DgsQuery
    public List<EventAnalysisOut> getEventAnalysisList(
            @InputArgument Instant from,
            @InputArgument Instant to,
            @InputArgument List<String> genres)
    {
        if (genres != null && genres.size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }

        logger.error("getEventAnalysisList");
        return eventApi.getEventAnalysisList(from, to, genres, getExtUserId());
    }

    @DgsQuery
    public List<EventWithTicketOut> getUpcomingEventsWithTicketInfo() {
        List<EventSort> sort = List.of(new EventSort(EventSortField.START_DATE, SortDirection.ASC));

        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(EventScope.PUBLIC)
                .extUserId(null)
                .genres(List.of())
                .dateFilter(DateFilterEnum.UPCOMING)
                .onlyVisible(true)
                .page(0)
                .size(MAX_UPCOMING_EVENT_SIZE)
                .sort(sort)
                .search(null)
                .build();

        EventConnection conn = eventApi.getEvents(criteria);

        return (conn.getNodes() == null ? List.<EventSummaryOut>of() : conn.getNodes())
                .stream()
                .map(this::toEventWithTicketOut)
                .toList();
    }

    @DgsQuery
    public List<EventWithTicketOut> getNewsRelatedEventsWithTicketInfo(@InputArgument String newsId) {

        List<Long> relatedEventIds = newsApi.getRelatedEventIds(newsId);
        List<EventSummaryOut> eventSummaryOuts = eventApi.getNewsRelatedEventsWithTicketInfo(relatedEventIds);

        return eventSummaryOuts.stream().map(this::toEventWithTicketOut).toList();
    }

    private EventWithTicketOut toEventWithTicketOut(EventSummaryOut e) {
        if (e == null) return EventWithTicketOut.builder().build();

        return EventWithTicketOut.builder()
                .eventId(e.getEventId())
                .name(e.getName())
                .startDate(e.getStartDate())
                .endDate(e.getEndDate())
                .imageUrl(e.getImageUrl())
                .maxImagePixel(e.getMaxImagePixel())
                .ownerUserIds(e.getOwnerUserIds())
                .venueUserId(e.getVenueUserId())
                .addressId(e.getAddressId())
                .build();
    }

    @DgsQuery
    public List<ActiveEventInfo> getActiveEvents(
            @InputArgument EventFilter filter,
            @InputArgument List<EventSort> sort
    ) {
        if (filter != null && filter.getGenres() != null &&
                filter.getGenres().size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }

        logger.error("getActiveEvents: filter: {}", filter);

        // getPublicEvents ile aynı criteria üretimi
        EventSearchCriteria in = EventSearchMapper.toCriteria(filter, null, null, sort, null);
        EventSearchCriteria criteria = EventSearchCriteria.builder()
                .scope(EventScope.PUBLIC)                // Public aktif eventler
                .extUserId(null)
                .genres(in.getGenres())
                .dateFilter(in.getDateFilter())          // (UPCOMING/ALL/PREVIOUS) – genelde UPCOMING beklenir
                .onlyVisible(in.getOnlyVisible())
                .page(in.getPage())
                .size(in.getSize())
                .sort(in.getSort())
                .search(in.getSearch())
                .build();

        return eventApi.getActiveEvents(criteria);
    }

    private String getExtUserId() {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth == null) {
            throw new RuntimeException("Authentication is null");
        }

        JwtAuthenticationToken jwtAuth = (JwtAuthenticationToken) auth;

        if (jwtAuth.getToken() == null) {
            throw new RuntimeException("JWT Token is null");
        }

        Jwt jwt = jwtAuth.getToken();
        String extUserId = userHelper.getExtUserId(jwt.getSubject());
        logger.info("extUserId: " + extUserId);
        if (extUserId == null) {
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Invalid token");
        }
        return extUserId;
    }

}