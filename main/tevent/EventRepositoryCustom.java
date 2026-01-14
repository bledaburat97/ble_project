@Override
public List<EventSummaryProjection> getAllEvents(
        List<String> genres,
        Boolean isVisible,
        CustomPageable pageable,
        DateFilterEnum dateFilterEnum,
        List<EventSort> sort,
        String keyword
) {
    Map<String, Object> paramMap = new HashMap<>();

    String whereCondition = NativeQuerySql.EVENT.APPROVED_EVENT_CONDITION;
    String query = NativeQuerySql.EVENT.BASE_GET_EVENT_QUERY.replace("{CONDITION}", whereCondition);

    if (isVisible != null) {
        query += " AND e.is_visible = :isVisible ";
        paramMap.put("isVisible", isVisible);
    }

    query = insertDateFilter(dateFilterEnum, query, paramMap);

    if (genres != null && !genres.isEmpty()) {
        query += NativeQuerySql.EVENT.EVENT_SUMMARY_GENRE_CONDITION;
        paramMap.put("genres", genres);
    }

    // ✅ PUBLIC SEARCH (name + description)
    if (keyword != null && !keyword.isBlank()) {
        query += """
            AND (
                LOWER(e.name) LIKE LOWER(:keyword)
                OR LOWER(e.description) LIKE LOWER(:keyword)
            )
        """;
        paramMap.put("keyword", "%" + keyword.trim() + "%");
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
        Boolean isVisible,
        String keyword
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

    // ✅ PUBLIC SEARCH COUNT
    if (keyword != null && !keyword.isBlank()) {
        query += """
            AND (
                LOWER(e.name) LIKE LOWER(:keyword)
                OR LOWER(e.description) LIKE LOWER(:keyword)
            )
        """;
        paramMap.put("keyword", "%" + keyword.trim() + "%");
    }

    Query nativeQuery = entityManager.createNativeQuery(query);
    paramMap.forEach(nativeQuery::setParameter);

    Object result = nativeQuery.getSingleResult();
    return result != null ? ((Number) result).intValue() : 0;
}
