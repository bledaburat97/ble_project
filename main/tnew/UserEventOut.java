@Getter
@Builder
public class UserEventOut {
    String username;
    String displayName;
    String userImageUrl;
    List<EventOut> events;
    Integer maxImagePixel;
}