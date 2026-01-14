public interface EventRepositoryCustom {

    List<EventSummaryProjection> getAllEvents(
            List<String> genres,
            Boolean isVisible,
            CustomPageable pageable,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort);

    int getAllEventsCount(
            List<String> genres,
            DateFilterEnum dateFilterEnum,
            Boolean isVisible
    );


    List<EventSummaryProjection> searchEvent(String searchTerm, int limit);

    List<EventSummaryProjection> getEventsByRole(
            RoleEnum role,
            EventStatus desiredStatus,
            UUID userId,
            String keyword,
            CustomPageable pageable,
            DateFilterEnum dateFilterEnum,
            boolean getOnlyVenueIsNotOwnerEvents,
            List<String> genres,
            List<EventSort> sort
    );

    Integer getEventsByRoleCount(
            RoleEnum role,
            EventStatus desiredStatus,
            UUID userId,
            String keyword,
            DateFilterEnum dateFilterEnum,
            boolean getOnlyVenueIsNotOwnerEvents,
            List<String> genres
    );

    List<EventSummaryProjection> getActiveEvents(
            Boolean onlyVisible,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort
    );

    List<EventSummaryProjection> getActiveEventsByIds(
            List<Long> eventIds,
            Boolean onlyVisible,
            DateFilterEnum dateFilterEnum,
            List<EventSort> sort,
            Integer limit
    );

}