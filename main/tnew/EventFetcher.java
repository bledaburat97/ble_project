@DgsComponent
public class EventFetcher {

    private final EventApi eventApi;
    private final NewsApi newsApi;
    private final UserHelper userHelper;

    public EventFetcher(EventApi eventApi, NewsApi newsApi, UserHelper userHelper) {
        this.eventApi = eventApi;
        this.newsApi = newsApi;
        this.userHelper = userHelper;
    }

    @DgsQuery
    public EventOut event(@InputArgument String eventId) {
        return eventApi.getEvent(eventId);
    }

    @PreAuthorize(
    "(#scope == null || #scope.name() == 'PUBLIC') " +
    "or (#scope.name() == 'OWNED' and hasAnyRole('VENUE','ORGANIZER')) " +
    "or (#scope.name() == 'PARTICIPATING' and hasAnyRole('VENUE','ARTIST'))"
    )
    @DgsQuery
    public EventConnection events(
            @InputArgument EventScope scope,
            @InputArgument EventFilter filter,
            @InputArgument Integer page,
            @InputArgument Integer size,
            @InputArgument List<EventSort> sort,
            @InputArgument String search
    ) {
        String extUserId = (scope == null || scope == EventScope.PUBLIC) ? null : getExtUserId();
        return eventApi.getEvents(scope, filter, page, size, sort, search, extUserId);
    }

    @DgsQuery
    public List<EventOut> activeEvents(@InputArgument EventFilter filter, @InputArgument List<EventSort> sort) {
        return eventApi.activeEvents(filter, sort);
    }

    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    public List<EventOut> eventInfoList(@InputArgument List<String> eventIds) {
        return eventApi.eventInfoList(eventIds);
    }

    @PreAuthorize("isAuthenticated()")
    @DgsQuery
    public List<EventOut> searchEvents(@InputArgument String keyword) {
        if (keyword == null || keyword.isBlank()
                || keyword.length() < ValidationConstant.USER.SEARCH_MIN_LENGTH
                || keyword.length() > ValidationConstant.USER.SEARCH_MAX_LENGTH) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid keyword length");
        }
        return eventApi.searchEvents(keyword);
    }

    // SearchOut entity fetcher (federation)
    @DgsEntityFetcher(name = "SearchOut")
    public SearchOut resolveSearchOut(Map<String, Object> values) {
        String keyword = (String) values.get("keyword");
        if (keyword == null || keyword.isBlank()) {
            throw new IllegalArgumentException("Missing keyword for resolving SearchOut");
        }
        SearchOut out = new SearchOut();
        out.setEvents(eventApi.searchEvents(keyword)); // artık Event list
        return out;
    }

    @PreAuthorize("hasAnyRole('VENUE','ORGANIZER')")
    @DgsQuery
    public EventOutDashboard getSignedInUserEventDetail(@InputArgument String eventId) {
        if (eventId == null || eventId.isBlank()) {
            throw new IllegalArgumentException("eventId cannot be null or blank");
        }
        return eventApi.getSignedInUserEventDetail(getExtUserId(), eventId);
    }


    @DgsQuery
    public UserEventOut userEventsByUsername(
            @InputArgument String username,
            @InputArgument Integer page,
            @InputArgument Integer size,
            @InputArgument DateFilterEnum dateFilterEnum
    ) {
        if (username == null || username.isBlank()) {
            throw new IllegalArgumentException("username cannot be null or blank");
        }
        int p = (page == null ? 0 : page);
        int s = (size == null ? 12 : size);
        DateFilterEnum df = (dateFilterEnum == null ? DateFilterEnum.ALL : dateFilterEnum);
        return eventApi.userEventsByUsername(username, p, s, df);
    }

    @PreAuthorize("hasAnyRole('VENUE','ORGANIZER')")
    @DgsQuery
    public List<EventAnalysisOut> eventAnalysisList(
            @InputArgument Instant from,
            @InputArgument Instant to,
            @InputArgument List<String> genres
    ) {
        // detaylı genre validation serviste de yapılacak ama burada hızlı guard kalsın
        if (genres != null && genres.size() > ValidationConstant.GENRE.MAX) {
            throw new IllegalArgumentException(ApiErrorCodes.INVALID_INPUT);
        }
        return eventApi.eventAnalysisList(from, to, genres, getExtUserId());
    }

    

    @DgsQuery
    public List<EventWithTicketOut> getUpcomingEventsWithTicketInfo() {
        return eventApi.getUpcomingEventsWithTicketInfo(MAX_UPCOMING_EVENT_SIZE);
    }

    @DgsQuery
    public List<EventWithTicketOut> getNewsRelatedEventsWithTicketInfo(@InputArgument String newsId) {
        if (newsId == null || newsId.isBlank()) {
            throw new IllegalArgumentException("newsId cannot be null or blank");
        }
        return eventApi.getNewsRelatedEventsWithTicketInfo(newsId, MAX_UPCOMING_EVENT_SIZE);
    }


    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    public EventDetailsOut eventDetails(@InputArgument String eventId, @InputArgument String extUserId) {
        return eventApi.getEventDetails(eventId, extUserId);
    }

    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    public List<String> businessUserEventIds(@InputArgument UUID userId) {
        return eventApi.getBusinessUserEventIds(userId);
    }


    private String getExtUserId() {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (!(auth instanceof JwtAuthenticationToken jwtAuth) || jwtAuth.getToken() == null) {
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Invalid token");
        }
        Jwt jwt = jwtAuth.getToken();
        String extUserId = userHelper.getExtUserId(jwt.getSubject());
        if (extUserId == null) throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Invalid token");
        return extUserId;
    }
}
