@AllArgsConstructor
@NoArgsConstructor
@Builder
@Getter
public class EventConnection {
    private Integer totalCount;
    private PageInfo pageInfo;
    private List<EventOut> nodes;
}