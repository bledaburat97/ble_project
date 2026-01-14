@DgsComponent
@RequiredArgsConstructor
public class EventDataFetcher {

  private final EventApi eventApi;

  @DgsQuery
  public EventOut getEventById(@InputArgument String eventId) {
    return eventApi.getEvent(eventId);
  }

  @PreAuthorize(
      "(#scope == null || #scope.name() == 'PUBLIC') " +
      "or (#scope.name() == 'OWNED' and hasAnyRole('VENUE','ORGANIZER')) " +
      "or (#scope.name() == 'PARTICIPATING' and hasAnyRole('VENUE','ARTIST'))"
  )
  @DgsQuery
  public EventConnection getEvents(
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

  @PreAuthorize("hasAnyRole('INTERNAL')")
  @DgsQuery
  public List<EventOut> getEventsByIds(@InputArgument List<String> eventIds) {
    return eventApi.eventInfoList(eventIds);
  }
}