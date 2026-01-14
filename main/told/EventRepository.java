@Repository
public interface EventRepository extends JpaRepository<Event, Long>, EventRepositoryCustom {

    @Query(nativeQuery = true, value = NativeQuerySql.EVENT.IS_EVENT_EDITABLE)
    Boolean isEventEditable(@Param("eventId") Long eventId, @Param("ownerUserId") UUID ownerUserId);


    @Query(value = """
        select e.event_id
        from event e
        where e.status = :eventStatus
            and exists (
                select 1
                from jsonb_array_elements_text(CAST(e.owner_user_ids AS jsonb)) elem
                where CAST(elem AS uuid) = :userId
          )
        """,
            nativeQuery = true)
    List<Long> findEventIdsByOwnerAndStatus(
            @Param("userId") UUID userId,
            @Param("eventStatus") EventStatus eventStatus
    );

}