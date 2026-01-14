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

    // Required IDs for federation/field resolvers
    @Builder.Default
    private List<String> ownerUserIds = List.of();

    private String venueUserId;
    private String addressId;

    @Builder.Default
    private List<String> artistIds = List.of();

    private List<String> genres;
    private List<String> otherArtists;

    // resolved in other services (can be null/empty here)
    private List<UserSummaryOut> organizers;
    private AddressOut address;
    private List<UserSummaryOut> artists;
    private VenueUserSummaryOut venue;

    private List<TicketDetailsOut> tickets;
    private Boolean hasTicket;

    private Instant filterFrom;
    private Instant filterTo;
}


@Getter
@Builder
public class AddressOut {
    private String googlePlaceId;
    private String description;
    private String fullAddress;
    private String country;
    private String administrativeArea;
    private String locality;
    private String postalCode;
    private String district;
    private String detailedAddress;
    private String lat;
    private String lng;
}

@Builder(toBuilder = true)
@Data
@AllArgsConstructor
@NoArgsConstructor
public class SoldTicketSummaryOut {
    private UUID ticketId;
    private String ticketType;
    private BigDecimal ticketPrice;
    private BigDecimal sellerFee;
    private BigDecimal serviceFee;
    private Currency currency;
    private Integer capacity;
    private Integer remainingCapacity;
    private Instant filterFrom;
    private Instant filterTo;
    private List<BookedTicketInstantInfo> bookedTickets;
}

@Builder
@Getter
public class TicketOutDashboard {
    private UUID ticketId;
    private String ticketType;
    private String ticketDescription;
    private BigDecimal ticketPrice;
    private BigDecimal sellerFee;
    private BigDecimal serviceFee;
    private Currency currency;
    private Integer capacity;
    private Integer remainingCapacity;
    private Integer limitPerUser;
    private Instant saleStartDate;
    private Instant saleEndDate;
    private boolean saleEndDateVisible;
}

@Builder
@Getter
public class TicketDetailsOut {
    private UUID ticketId;
    private String ticketType;
    private String ticketDescription;
    private BigDecimal ticketPrice;
    private BigDecimal sellerFee;
    private BigDecimal serviceFee;
    private Currency currency;
    private Integer capacity;
    private Integer remainingCapacity;
    private Integer limitPerUser;
    private Instant saleStartDate;
    private Instant saleEndDate;
    private boolean saleEndDateVisible;
    private Instant filterFrom;
    private Instant filterTo;
    private List<BookedTicketInstantInfo> bookedTickets;
}

@Builder
@Data
@AllArgsConstructor
@NoArgsConstructor
public class BookedTicketInstantInfo {
    private Instant boughtTime; //saat ve gün bilgisi
    private Integer count;
}