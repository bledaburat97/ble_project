@Repository
@RequiredArgsConstructor
public class EventRepositoryCustomImpl implements EventRepositoryCustom {

    private final EntityManager entityManager;
    private final ObjectMapper objectMapper;

    @Override
    public List<EventSummaryProjection> getAllEvents(
            List<String> genres,
            Boolean isVisible,
            CustomPageable pageable,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort
    ) {
        Map<String, Object> paramMap = new HashMap<>();

        String whereCondition = NativeQuerySql.EVENT.APPROVED_EVENT_CONDITION;
        String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        // optional visible
        if (isVisible != null) {
            query += " AND e.is_visible = :isVisible ";
            paramMap.put("isVisible", isVisible);
        }

        // date
        query = insertDateFilter(dateFilterEnum, query, paramMap);

        // genres (opsiyonel)
        if (genres != null && !genres.isEmpty()) {
            query += NativeQuerySql.EVENT.EVENT_SUMMARY_GENRE_CONDITION;
            paramMap.put("genres", genres);
        }

        query += resolveOrderBy(sort, dateFilterEnum);

        Query nativeQuery = entityManager.createNativeQuery(query, Tuple.class)
                .setFirstResult(pageable.page() * pageable.size())
                .setMaxResults(pageable.size());

        paramMap.forEach(nativeQuery::setParameter);
        @SuppressWarnings("unchecked")
        List<Tuple> tuples = (List<Tuple>) nativeQuery.getResultList();
        return tuplesToProjection(tuples);
    }

    @Override
    public int getAllEventsCount(
            List<String> genres,
            DateFilterEnum dateFilterEnum,
            Boolean isVisible
    ) {
        Map<String, Object> paramMap = new HashMap<>();

        String whereCondition = NativeQuerySql.EVENT.APPROVED_EVENT_CONDITION;
        String query = NativeQuerySql.EVENT.BASE_COUNT_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        if (isVisible != null) {
            query += " AND e.is_visible = :isVisible ";
            paramMap.put("isVisible", isVisible);
        }

        query = insertDateFilter(dateFilterEnum, query, paramMap);

        if (genres != null && !genres.isEmpty()) {
            query += NativeQuerySql.EVENT.EVENT_SUMMARY_GENRE_CONDITION;
            paramMap.put("genres", genres);
        }

        Query nativeQuery = entityManager.createNativeQuery(query);
        paramMap.forEach(nativeQuery::setParameter);
        Object result = nativeQuery.getSingleResult();
        return result != null ? ((Number) result).intValue() : 0;
    }

    @Override
    public List<EventSummaryProjection> getEventsByRole(
            RoleEnum role,
            EventStatus desiredStatus,
            UUID userId,
            String keyword,
            CustomPageable pageable,
            DateFilterEnum dateFilterEnum,
            boolean getOnlyVenueIsNotOwnerEvents,
            List<String> genres,
            List<EventSort> sort
    ) {
        Map<String, Object> paramMap = new HashMap<>() {{
            put("userId", userId);
        }};

        String whereCondition = switch (role) {
            case ORGANIZER -> NativeQuerySql.EVENT.ORGANIZER_USER_CONDITION;
            case VENUE     -> NativeQuerySql.EVENT.VENUE_USER_CONDITION;
            case ARTIST    -> NativeQuerySql.EVENT.ARTIST_USER_CONDITION;
            default        -> throw new IllegalArgumentException("Unsupported role: " + role);
        };

        String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        query = insertDateFilter(dateFilterEnum, query, paramMap);

        if (desiredStatus != null) {
            query += NativeQuerySql.EVENT.EVENT_STATUS_CONDITION;
            paramMap.put("status", desiredStatus.name());
        }

        if (getOnlyVenueIsNotOwnerEvents) {
            query += NativeQuerySql.EVENT.VENUE_IS_NOT_EVENT_OWNER;
        }

        if (keyword != null && !keyword.isEmpty()) {
            query += NativeQuerySql.EVENT.USER_EVENTS_SEARCH_CONDITION;
            paramMap.put("keyword", "%" + keyword + "%");
        }

        if (genres != null && !genres.isEmpty()) {
            query += NativeQuerySql.EVENT.EVENT_SUMMARY_GENRE_CONDITION;
            paramMap.put("genres", genres);
        }

        query += resolveOrderBy(sort, dateFilterEnum);

        Query nativeQuery = entityManager.createNativeQuery(query, Tuple.class);

        if (pageable != null) {
            nativeQuery
                    .setFirstResult(pageable.page() * pageable.size())
                    .setMaxResults(pageable.size());
        }

        paramMap.forEach(nativeQuery::setParameter);

        @SuppressWarnings("unchecked")
        List<Tuple> tuples = (List<Tuple>) nativeQuery.getResultList();
        return tuplesToProjection(tuples);
    }



    @Override
    public Integer getEventsByRoleCount(RoleEnum role, EventStatus desiredStatus,
                                       UUID userId, String keyword, DateFilterEnum dateFilterEnum, boolean getOnlyVenueIsNotOwnerEvents, List<String> genres) {
        Map<String, Object> paramMap = new HashMap<>() {{
            put("userId", userId);
        }};
        String whereCondition = "";
        if(role == RoleEnum.ORGANIZER) {
            whereCondition = NativeQuerySql.EVENT.ORGANIZER_USER_CONDITION;
        }
        else if(role == RoleEnum.VENUE) {
            whereCondition = NativeQuerySql.EVENT.VENUE_USER_CONDITION;
        }
        else if(role == RoleEnum.ARTIST) {
            whereCondition = NativeQuerySql.EVENT.ARTIST_USER_CONDITION;
        }

        String query = NativeQuerySql.EVENT.BASE_COUNT_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        query = insertDateFilter(dateFilterEnum, query, paramMap);

        if (desiredStatus != null) {
            query += NativeQuerySql.EVENT.EVENT_STATUS_CONDITION;
            paramMap.put("status", desiredStatus.name());
        }

        if (getOnlyVenueIsNotOwnerEvents) {
            query += NativeQuerySql.EVENT.VENUE_IS_NOT_EVENT_OWNER;
        }

        if (keyword != null && !keyword.isEmpty()) {
            query += NativeQuerySql.EVENT.USER_EVENTS_SEARCH_CONDITION;
            paramMap.put("keyword", "%" + keyword + "%");
        }

        if (genres != null && !genres.isEmpty()) {
            query += NativeQuerySql.EVENT.EVENT_SUMMARY_GENRE_CONDITION; // IN (:genres)
            paramMap.put("genres", genres);
        }

        Query nativeQuery = entityManager.createNativeQuery(query);
        paramMap.forEach(nativeQuery::setParameter);
        Object result = nativeQuery.getSingleResult();
        return result != null ? ((Number) result).intValue() : 0;
    }

    @Override
    public List<EventSummaryProjection> getActiveEvents(
            Boolean onlyVisible,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort
    ) {
        Map<String, Object> paramMap = new HashMap<>();

        String whereCondition = NativeQuerySql.EVENT.APPROVED_EVENT_CONDITION;
        String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        if (onlyVisible != null) {
            query += " AND e.is_visible = :isVisible ";
            paramMap.put("isVisible", onlyVisible);
        }

        query = insertDateFilter(dateFilterEnum, query, paramMap);

        query += resolveOrderBy(sort, dateFilterEnum);

        Query nativeQuery = entityManager.createNativeQuery(query, Tuple.class);

        paramMap.forEach(nativeQuery::setParameter);

        @SuppressWarnings("unchecked")
        List<Tuple> tuples = (List<Tuple>) nativeQuery.getResultList();
        return tuplesToProjection(tuples);
    }




    @Override
    public List<EventSummaryProjection> searchEvent(
            String searchTerm, int limit) {

        String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", NativeQuerySql.EVENT.SEARCH_EVENT_CONDITION);

        Map<String, Object> paramMap = new HashMap<>() {{
            put("searchTerm", "%" + searchTerm + "%");
        }};
        return readResultListToEventSummaryProjectionList(query, paramMap, 0, limit, DateFilterEnum.ALL);
    }

    @Override
    public List<EventSummaryProjection> getActiveEventsByIds(
            List<Long> eventIds,
            Boolean onlyVisible,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort,
            Integer limit
    ) {
        if (eventIds == null || eventIds.isEmpty()) return List.of();

        Map<String, Object> paramMap = new HashMap<>();

        String whereCondition = NativeQuerySql.EVENT.APPROVED_EVENT_CONDITION;
        String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", whereCondition);

        query += " AND e.event_id IN (:eventIds) ";
        paramMap.put("eventIds", eventIds);

        if (onlyVisible != null) {
            query += " AND e.is_visible = :isVisible ";
            paramMap.put("isVisible", onlyVisible);
        }

        query = insertDateFilter(dateFilterEnum, query, paramMap);
        query += resolveOrderBy(sort, dateFilterEnum);

        Query nativeQuery = entityManager.createNativeQuery(query, Tuple.class);

        if (limit != null && limit > 0) {
            nativeQuery.setMaxResults(limit);
        }

        paramMap.forEach(nativeQuery::setParameter);

        @SuppressWarnings("unchecked")
        List<Tuple> tuples = (List<Tuple>) nativeQuery.getResultList();
        return tuplesToProjection(tuples);
    }
    
    @SuppressWarnings("unchecked")
    private List<EventSummaryProjection> readResultListToEventSummaryProjectionList(
            String query,
            Map<String, Object> paramMap,
            int page,
            int size,
            DateFilterEnum dateFilterEnum
    ) {
        query += dateFilterEnum.equals(DateFilterEnum.PREVIOUS)
                ? NativeQuerySql.EVENT.ORDER_BY_START_DATE_DESC
                : NativeQuerySql.EVENT.ORDER_BY_START_DATE_ASC;

        Query nativeQuery = entityManager
                .createNativeQuery(query, Tuple.class)
                .setFirstResult(page * size)
                .setMaxResults(size);

        paramMap.forEach(nativeQuery::setParameter);

        List<Tuple> tuples = (List<Tuple>) nativeQuery.getResultList();

        return tuples.stream().map(tuple -> {
            List<UUID> ownerUserIds = parseOwnerUserIds(tuple.get("owner_user_ids"));

            return new EventSummaryProjection(
                    (Long) tuple.get("event_id"),
                    (String) tuple.get("name"),
                    (String) tuple.get("description"),
                    (Instant) tuple.get("start_date"),
                    (Instant) tuple.get("end_date"),
                    Enum.valueOf(EventStatus.class, (String) tuple.get("status")),
                    (UUID) tuple.get("image_id"),
                    ownerUserIds,
                    (UUID) tuple.get("venue_id"),
                    (UUID) tuple.get("address_id"),
                    (Integer) tuple.get("max_image_pixel"),
                    (Boolean) tuple.get("is_visible")
            );
        }).toList();
    }

    private List<UUID> parseOwnerUserIds(Object raw) {
        if (raw == null) {
            return List.of();
        }

        try {
            // PostgreSQL + Hibernate genellikle PGobject döndürür
            if (raw instanceof org.postgresql.util.PGobject pg) {
                String json = pg.getValue();
                return objectMapper.readValue(json, new com.fasterxml.jackson.core.type.TypeReference<List<UUID>>() {});
            }

            // Bazı durumlarda direkt String gelebilir
            if (raw instanceof String s) {
                return objectMapper.readValue(s, new com.fasterxml.jackson.core.type.TypeReference<List<UUID>>() {});
            }

            // İleride Hibernate farklı tip dönerse buradan görürsün
            throw new IllegalStateException("Unexpected type for owner_user_ids: " + raw.getClass());
        } catch (Exception e) {
            throw new RuntimeException("Failed to parse owner_user_ids json", e);
        }
    }


    private String resolveOrderBy(List<EventSort> sort, DateFilterEnum dateFilterEnum) {
        if (sort == null || sort.isEmpty()) {
            if (dateFilterEnum == DateFilterEnum.PREVIOUS) {
                return NativeQuerySql.EVENT.ORDER_BY_START_DATE_DESC;
            } else {
                return NativeQuerySql.EVENT.ORDER_BY_START_DATE_ASC;
            }
        }
        StringBuilder sb = new StringBuilder(" ORDER BY ");
        for (int i = 0; i < sort.size(); i++) {
            EventSort s = sort.get(i);
            String col = switch (s.getField()) {
                case START_DATE -> "e.start_date";
                case CREATED_AT -> "e.created_at";
                case NAME -> "e.name";
            };
            sb.append(col).append(" ").append(s.getDir() == SortDirection.DESC ? "DESC" : "ASC");
            if (i < sort.size() - 1) sb.append(", ");
        }
        return sb.toString();
    }

    private String insertDateFilter(DateFilterEnum dateFilterEnum, String query, Map<String,Object> paramMap) {
        if (dateFilterEnum == null || dateFilterEnum == DateFilterEnum.ALL) return query;

        query += (dateFilterEnum == DateFilterEnum.UPCOMING)
                ? " AND e.end_date > :currentDate "
                : " AND e.end_date <= :currentDate ";

        paramMap.put("currentDate", java.sql.Timestamp.from(Instant.now())); // native için Timestamp güvenli
        return query;
    }

    @SuppressWarnings("unchecked")
    private List<EventSummaryProjection> tuplesToProjection(List<Tuple> tuples){
        return tuples.stream().map(t -> {
            List<UUID> ownerUserIds = parseOwnerUserIds(t.get("owner_user_ids"));
            return new EventSummaryProjection(
                    (Long) t.get("event_id"),
                    (String) t.get("name"),
                    (String) t.get("description"),
                    (Instant) t.get("start_date"),
                    (Instant) t.get("end_date"),
                    Enum.valueOf(EventStatus.class, (String) t.get("status")),
                    (UUID) t.get("image_id"),
                    ownerUserIds,
                    (UUID) t.get("venue_id"),
                    (UUID) t.get("address_id"),
                    (Integer) t.get("max_image_pixel"),
                    (Boolean) t.get("is_visible")
            );
        }).toList();
    }
}