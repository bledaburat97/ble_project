public record EventInfo(
        String eventId,
        String eventName,
        Instant startDate,
        Instant endDate,
        String imageId
) {}

@AllArgsConstructor
@Getter
@Builder
public class EventSummaryOut {
    private String eventId;
    private String name;
    private String description;
    private Instant startDate;
    private Instant endDate;
    private String imageUrl;
    private String locality;
    private String administrativeArea;
    private String country;
    private String ownerUserIds;
    private String venueUserId;
    private String addressId;
    private Integer maxImagePixel;
    private List<UserSummaryOut> organizers;
    private Boolean isVisible;
    private EventStatus eventStatus;
    private Boolean hasTicket;
}

@Getter
@Builder
public class EventOut {
    private String eventId;
    private String name;
    private String description;
    private List<String> genres;
    private List<UserSummaryOut> organizers;
    private AddressOut address;
    private List<UserSummaryOut> artists;
    private VenueUserSummaryOut venue;
    private List<String> otherArtists;
    private List<TicketOut> tickets;
    private Instant startDate;
    private Instant endDate;
    private String imageUrl;
    private String ownerUserIds;
    private String venueUserId;
    private String artistIds;
    private String addressId;
    private Integer maxImagePixel;
    private EventStatus eventStatus;
}

@AllArgsConstructor
@Getter
@Builder
public class EventOutDashboard  {
    private String eventId;
    private String name;
    private String description;
    private List<String> genres;
    private List<UserSummaryOut> organizers;
    private AddressOut address;
    private List<UserSummaryOut> artists;
    private VenueUserSummaryOut venue;
    private List<String> otherArtists;
    private List<TicketOutDashboard> tickets;
    private Instant startDate;
    private Instant endDate;
    private String imageUrl;
    private EventStatus eventStatus;
    private String ownerUserIds;
    private String venueUserId;
    private String artistIds;
    private String addressId;
    private Integer maxImagePixel;
    private Boolean isVisible;
}



@Data
@AllArgsConstructor
@NoArgsConstructor
@Builder
public class EventAnalysisOut {
    private String eventId;
    private String name;
    private Instant startDate;
    private Instant endDate;
    private String imageUrl;
    private Integer maxImagePixel;
    private EventStatus eventStatus;
    private Instant filterFrom;
    private Instant filterTo;
    private List<SoldTicketSummaryOut> soldTickets;
}

@Getter
@Builder
public class EventWithTicketOut {
    private String eventId;
    private String name;
    private Instant startDate;
    private Instant endDate;
    private String imageUrl;
    private Integer maxImagePixel;
    private String ownerUserIds;
    private String venueUserId;
    private String addressId;
    private String locality;
    private String administrativeArea;
    private String country;
    private List<UserSummaryOut> organizers;
    private TicketOut ticket;
}



@AllArgsConstructor
@Getter
@Builder
public class ActiveEventInfo {
    private String eventId;
    private String locality;
    private String ownerUserIds;
    private String venueUserId;
    private String addressId;
    private List<UserSummaryOut> organizers;
    private VenueUserSummaryOut venue;
}


@AllArgsConstructor @NoArgsConstructor @Builder @Getter
public class EventConnection {
    private Integer totalCount;
    private PageInfo pageInfo;
    private List<EventSummaryOut> nodes;
}

@Getter
@Builder
public class UserEventOut {
    String username;
    String displayName;
    String userImageUrl;
    List<EventSummaryOut> events;
    Integer maxImagePixel;
}

@Getter
@Builder
public class EventDetailsOut {
    private String eventId;
    private Instant startDate;
    private Instant endDate;
    private String ownerBusinessUserId;
    private Boolean isEventOwner;
}
