@DgsComponent
public class EventDataFetcher {

    private final EventApi eventApi;
    private final NewsApi newsApi;

    private UserHelper userHelper;

    private static final Logger logger = LoggerFactory.getLogger(EventDataFetcher.class);

    public EventDataFetcher(EventApi eventApi, UserHelper userHelper, NewsApi newsApi) {
        this.eventApi = eventApi;
        this.userHelper = userHelper;
        this.newsApi = newsApi;
    }

    @Autowired
    CSVService csvService;

    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    List<EventInfo> getEventInfoList(@InputArgument List<String> eventIds) {
        if (eventIds == null || eventIds.isEmpty()) return List.of();
        return eventApi.getEventInfoList(eventIds);
    }

    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    EventSummaryOut getEventInfo(@InputArgument String eventId) {
        logger.info("getEventInfo: {}", eventId);
        if (eventId == null || eventId.isBlank()) {
            throw new IllegalArgumentException("eventId cannot be null or blank");
        }
        return eventApi.getEventInfo(eventId);
    }

    @DgsQuery
    public EventOut getEvent(@InputArgument String eventId) {
        logger.info("Received request to fetch event with ID: {}", eventId);
        return eventApi.getEvent(eventId);
    }

    @DgsQuery
    public UserEventOut searchUserEventsByUsername(
            @InputArgument Integer page,
            @InputArgument Integer size,
            @InputArgument String username,
            @InputArgument DateFilterEnum dateFilterEnum
    ) {
        if (username == null || username.isBlank()) {
            throw new IllegalArgumentException("Username cannot be null or blank");
        }
        int p = (page == null ? 0 : page);
        int s = (size == null ? 12 : size);
        DateFilterEnum df = (dateFilterEnum == null ? DateFilterEnum.ALL : dateFilterEnum);
        return eventApi.searchUserEventsByUsername(p, s, username, df);
    }

    @PreAuthorize("hasAnyRole('VENUE','ORGANIZER')")
    @DgsQuery
    public EventOutDashboard getSignedInUserEventDetail(@InputArgument String eventId) {
        if (eventId == null || eventId.isBlank()) {
            throw new IllegalArgumentException("eventId cannot be null or blank");
        }
        return eventApi.getSignedInUserEventDetail(getExtUserId(), eventId);
    }


    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    public EventDetailsOut getEventDetails(@InputArgument String eventId, @InputArgument String extUserId) {
        return eventApi.getEventDetails(eventId, extUserId);
    }

    @PreAuthorize("hasAnyRole('INTERNAL')")
    @DgsQuery
    public List<String> getBusinessUserEventIds(@InputArgument UUID userId){
        return eventApi.getBusinessUserEventIds(userId);
    }

    //search query
    @DgsEntityFetcher(name = "SearchOut")
    public SearchOut resolveSearchOut(Map<String, Object> values) {
        String keyword = (String) values.get("keyword");
        if (keyword == null) {
            throw new IllegalArgumentException("Missing keyword for resolving SearchOut");
        }

        SearchOut searchOut = new SearchOut();
        List<EventSummaryOut> events = eventApi.search(keyword);
        searchOut.setEvents(events);
        return searchOut;
    }

    @PreAuthorize("isAuthenticated()")
    @DgsQuery
    public List<EventSummaryOut> searchEvents(@InputArgument String keyword) {
        if (keyword == null || keyword.isBlank() || keyword.length() < ValidationConstant.USER.SEARCH_MIN_LENGTH  || keyword.length() > ValidationConstant.USER.SEARCH_MAX_LENGTH ) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid keyword length");
        }

        return eventApi.search(keyword);
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