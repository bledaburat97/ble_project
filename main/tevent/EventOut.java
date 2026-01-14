import java.time.Instant;
import java.util.List;

@Getter
@Builder
@AllArgsConstructor
@NoArgsConstructor
public class EventOut {
    private String eventId;

    private String name;
    private String description;

    private Instant startDate;
    private Instant endDate;

    private String imageUrl;
    private Integer maxImagePixel;

    private Boolean isVisible;
    private EventStatus eventStatus;
    private Boolean hasTicket;

    @Builder.Default
    private List<String> ownerUserIds = List.of();

    private String venueUserId;
    private String addressId;

    @Builder.Default
    private List<String> artistIds = List.of();

    private List<String> genres;
    private List<String> otherArtists;

    private List<UserSummaryOut> organizers;
    private AddressOut address;
    private List<UserSummaryOut> artists;
    private VenueUserSummaryOut venue;

    private List<TicketOut> tickets;
}