import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;

import main.tnew.AddressOut;
import main.transaction.ActiveEventInfo;
import main.transaction.EventOutDashboard;
import main.transaction.EventSummaryOut;
import main.transaction.EventWithTicketOut;

public Map<UUID, UserSummaryOut> getUsersSummaryOutByIds(Set<UUID> ids) {
    List<User> users = userRepository.findAllById(ids);

    return users.stream()
        .filter(u -> userHelper.isOrganizer(u.getRole().getRoleId()) || userHelper.isVenue(u.getRole().getRoleId()))
        .collect(Collectors.toMap(
            User::getUserId,
            u -> new UserSummaryOut(
                u.getUserId().toString(),
                u.getUsername(),
                u.getDisplayName(),
                storageHelper.getImageUrl(u.getProfilePhotoId()),
                u.getRole().getName()
            )
        ));
}

@DgsDataLoader
@RequiredArgsConstructor
public class UserSummaryOutLoader implements MappedBatchLoader<UUID, UserSummaryOut> {

    private final UserApi userApi;

    @Override
    public CompletionStage<Map<UUID, UserSummaryOut>> load(Set<UUID> keys) {
        if (keys == null || keys.isEmpty()) {
            return CompletableFuture.completedFuture(Map.of());
        }
        return CompletableFuture.completedFuture(userApi.getUsersSummaryOutByIds(keys));
    }
}

@DgsComponent
public class UserDataFetcher {
    private static final Logger logger = LoggerFactory.getLogger(UserDataFetcher.class);

    private final UserApi userApi;
    private final AddressRepository addressRepository;

    private final AddressMapper addressMapper;
    private final UserRepository userRepository;
    private final StorageHelper storageHelper;
    private final UserHelper userHelper;
    private final EncryptionUtil encryptionUtil;

    public UserDataFetcher(UserApi userApi, AddressRepository addressRepository, AddressMapper addressMapper, UserRepository userRepository, StorageHelper storageHelper, UserHelper userHelper, EncryptionUtil encryptionUtil) {
        this.userApi = userApi;
        this.addressRepository = addressRepository;
        this.addressMapper = addressMapper;
        this.userRepository = userRepository;
        this.storageHelper = storageHelper;
        this.userHelper = userHelper;
        this.encryptionUtil = encryptionUtil;
    }


    @DgsEntityFetcher(name = "EventOut")
    public EventOut resolveEventOut(Map<String, Object> values) {
        Object eventIdValue = values.get("eventId");
        if (eventIdValue == null) {
            throw new IllegalArgumentException("eventIdValue is missing or null");
        }
        String eventId = (String) eventIdValue;

        return EventOut.builder()
                .eventId(eventId)
                .build();
    }


    @DgsData(parentType = "EventOut", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForEventOut(
            DgsDataFetchingEnvironment dfe,
            Map<String, Object> representation
    ) {
        @SuppressWarnings("unchecked")
        List<String> ownerUserIds = (List<String>) representation.get("ownerUserIds");

        if (ownerUserIds == null || ownerUserIds.isEmpty()) {
            return CompletableFuture.completedFuture(List.of());
        }

        List<UUID> ids = ownerUserIds.stream().map(UUID::fromString).toList();

        DataLoader<UUID, UserSummaryOut> dl = dfe.getDataLoader(UserSummaryOutLoader.class);

        return dl.loadMany(ids).thenApply(list ->
                list.stream().filter(Objects::nonNull).toList()
        );
    }




    @DgsEntityFetcher(name = "EventOut")
    public EventOut resolveEventOut(Map<String, Object> values) {
        logger.info("Resolving EventOut with values:");
        values.forEach((key, value) -> {
            logger.info("Key: {}, Value: {}", key, value);
        });

        String ownerUserIdsRaw = (String) values.get("ownerUserIds");

        List<String> ownerUserIdsStr = new ArrayList<>();

        if (ownerUserIdsRaw != null && !ownerUserIdsRaw.isBlank()) {
            ownerUserIdsStr = Arrays.asList(ownerUserIdsRaw.split(","));
        }

        if (ownerUserIdsStr.isEmpty()) {
            throw new IllegalArgumentException("ownerUserIdValues are missing or null");
        }

        Object eventIdValue = values.get("eventId");
        if (eventIdValue == null) {
            throw new IllegalArgumentException("eventIdValue is missing or null");
        }
        String eventId = (String) eventIdValue;

        String venueUserId = Optional.ofNullable((String) values.get("venueUserId")).orElse(null);
        String artistIds = Optional.ofNullable((String) values.get("artistIds")).orElse(null);
        String addressId = Optional.ofNullable((String) values.get("addressId")).orElse(null);

        return EventOut.builder()
                .eventId(eventId)
                .ownerUserIds(ownerUserIdsRaw)
                .venueUserId(venueUserId)
                .artistIds(artistIds)
                .addressId(addressId)
                .build();
    }

    @DgsData(parentType = "EventOut", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForEventOut(DgsDataFetchingEnvironment dfe) {
        EventOut eventOut = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(UserSummaryOutListLoader.class);
        return fetchOrganizers(eventOut.getOwnerUserIds(), dataLoader);
    }

    @DgsData(parentType = "EventOut", field = "venue")
    public CompletableFuture<VenueUserSummaryOut> venueForEventOut(DgsDataFetchingEnvironment dfe) {
        EventOut eventOut = dfe.getSourceOrThrow();
        DataLoader<UUID, VenueUserSummaryOut> dataLoader = dfe.getDataLoader(VenueUserSummaryOutLoader.class);
        return fetchVenue(eventOut.getVenueUserId(), dataLoader);
    }

    @DgsData(parentType = "EventOut", field = "artists")
    public CompletableFuture<List<UserSummaryOut>> artistsForEventOut(DgsDataFetchingEnvironment dfe) {
        EventOut eventOut = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(ArtistUserSummaryOutListLoader.class);
        return fetchArtists(eventOut.getArtistIds(), dataLoader);
    }

    @DgsData(parentType = "EventOut", field = "address")
    public CompletableFuture<AddressOut> addressForEventOut(DgsDataFetchingEnvironment dfe) {
        EventOut eventOut = dfe.getSourceOrThrow();
        DataLoader<String, AddressOut> dataLoader = dfe.getDataLoader(AddressOutLoader.class);
        return fetchAddress(eventOut.getAddressId(), dataLoader);
    }

    //getActiveEvents graphql
    @DgsEntityFetcher(name = "ActiveEventInfo")
    public ActiveEventInfo resolveActiveEventInfo(Map<String, Object> values) {
        logger.error("Resolving ActiveEventInfo with values:");
        values.forEach((key, value) -> {
            logger.info("Key: {}, Value: {}", key, value);
        });
        String ownerUserIdsRaw = (String) values.get("ownerUserIds");
        List<String> ownerUserIdsStr = new ArrayList<>();
        if (ownerUserIdsRaw != null && !ownerUserIdsRaw.isBlank()) {
            ownerUserIdsStr = Arrays.asList(ownerUserIdsRaw.split(","));
        }
        if (ownerUserIdsStr.isEmpty()) {
            throw new IllegalArgumentException("ownerUserIdValues are missing or null");
        }

        Object eventIdValue = values.get("eventId");
        if (eventIdValue == null) {
            throw new IllegalArgumentException("eventIdValue is missing or null");
        }
        String eventId = (String) eventIdValue;

        UUID addressId;
        String venueUserIdStr = Optional.ofNullable((String) values.get("venueUserId")).orElse(null);

        if(venueUserIdStr != null) {
            UUID venueUserId = UUID.fromString((String) values.get("venueUserId"));
            User venueUser = userRepository.findById(venueUserId).orElseThrow(() -> new ValidationException(
                    "Venue User with ID %s not found".formatted(venueUserId),
                    ApiErrorCodes.USER_NOT_FOUND));
            addressId = venueUser.getAddress().getAddressId();
        }
        else if(values.get("addressId") != null){
            addressId = UUID.fromString((String) values.get("addressId"));
        }
        else{
            return ActiveEventInfo.builder().ownerUserIds(ownerUserIdsRaw).build();
        }

        AddressOut address = userApi.getAddressOut(addressId.toString());
        logger.error("Resolving ActiveEventInfo with values 2:");

        return ActiveEventInfo.builder()
                .eventId(eventId)
                .ownerUserIds(ownerUserIdsRaw)
                .venueUserId(venueUserIdStr)
                .locality(address.getLocality())
                .build();
    }

    @DgsData(parentType = "ActiveEventInfo", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForActiveEventInfo(DgsDataFetchingEnvironment dfe) {
        ActiveEventInfo activeEventInfo = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(UserSummaryOutListLoader.class);
        return fetchOrganizers(activeEventInfo.getOwnerUserIds(), dataLoader);
    }

    @DgsData(parentType = "ActiveEventInfo", field = "venue")
    public CompletableFuture<VenueUserSummaryOut> venueForActiveEventInfo(DgsDataFetchingEnvironment dfe) {
        ActiveEventInfo activeEventInfo = dfe.getSourceOrThrow();
        DataLoader<UUID, VenueUserSummaryOut> dataLoader = dfe.getDataLoader(VenueUserSummaryOutLoader.class);
        return fetchVenue(activeEventInfo.getVenueUserId(), dataLoader);
    }

    //getSignedInUserEventDetail graphql
    @DgsEntityFetcher(name = "EventOutDashboard")
    public EventOutDashboard resolveEventOutDashboard(Map<String, Object> values) {
        logger.info("Resolving EventOutDashboard with values:");
        values.forEach((key, value) -> {
            logger.info("Key: {}, Value: {}", key, value);
        });

        String ownerUserIdsRaw = (String) values.get("ownerUserIds");

        List<String> ownerUserIdsStr = new ArrayList<>();

        if (ownerUserIdsRaw != null && !ownerUserIdsRaw.isBlank()) {
            ownerUserIdsStr = Arrays.asList(ownerUserIdsRaw.split(","));
        }

        if (ownerUserIdsStr.isEmpty()) {
            throw new IllegalArgumentException("ownerUserIdValues are missing or null");
        }

        Object eventIdValue = values.get("eventId");
        if (eventIdValue == null) {
            throw new IllegalArgumentException("eventIdValue is missing or null");
        }
        String eventId = (String) eventIdValue;

        String venueUserId = Optional.ofNullable((String) values.get("venueUserId")).orElse(null);
        String artistIds = Optional.ofNullable((String) values.get("artistIds")).orElse(null);
        String addressId = Optional.ofNullable((String) values.get("addressId")).orElse(null);

        return EventOutDashboard.builder()
                .eventId(eventId)
                .ownerUserIds(ownerUserIdsRaw)
                .venueUserId(venueUserId)
                .artistIds(artistIds)
                .addressId(addressId)
                .build();
    }

    @DgsData(parentType = "EventOutDashboard", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForEventOutDashboard(DgsDataFetchingEnvironment dfe) {
        EventOutDashboard eventOutDashboard = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(UserSummaryOutListLoader.class);
        return fetchOrganizers(eventOutDashboard.getOwnerUserIds(), dataLoader);
    }

    @DgsData(parentType = "EventOutDashboard", field = "venue")
    public CompletableFuture<VenueUserSummaryOut> venueForEventOutDashboard(DgsDataFetchingEnvironment dfe) {
        EventOutDashboard eventOutDashboard = dfe.getSourceOrThrow();
        DataLoader<UUID, VenueUserSummaryOut> dataLoader = dfe.getDataLoader(VenueUserSummaryOutLoader.class);
        return fetchVenue(eventOutDashboard.getVenueUserId(), dataLoader);
    }

    @DgsData(parentType = "EventOutDashboard", field = "artists")
    public CompletableFuture<List<UserSummaryOut>> artistsForEventOutDashboard(DgsDataFetchingEnvironment dfe) {
        EventOutDashboard eventOutDashboard = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(ArtistUserSummaryOutListLoader.class);
        return fetchArtists(eventOutDashboard.getArtistIds(), dataLoader);
    }

    @DgsData(parentType = "EventOutDashboard", field = "address")
    public CompletableFuture<AddressOut> addressForEventOutDashboard(DgsDataFetchingEnvironment dfe) {
        EventOutDashboard eventOutDashboard = dfe.getSourceOrThrow();
        DataLoader<String, AddressOut> dataLoader = dfe.getDataLoader(AddressOutLoader.class);
        return fetchAddress(eventOutDashboard.getAddressId(), dataLoader);
    }

    //getEvents, getUserIncludedEvents, searchUserIncludedEvents, searchEvent query
    @DgsEntityFetcher(name = "EventWithTicketOut")
    public EventWithTicketOut resolveEventWithTicketOut(Map<String, Object> values) {
        String eventId = (String) values.get("eventId");

        String ownerUserIdsRaw = (String) values.get("ownerUserIds");
        List<String> ownerUserIdsStr = new ArrayList<>();

        if (ownerUserIdsRaw != null && !ownerUserIdsRaw.isBlank()) {
            ownerUserIdsStr = Arrays.asList(ownerUserIdsRaw.split(","));
        }

        if (ownerUserIdsStr.isEmpty()) {
            throw new IllegalArgumentException("ownerUserIdValues are missing or null");
        }

        UUID addressId;
        if(values.get("venueUserId") != null) {
            UUID venueUserId = UUID.fromString((String) values.get("venueUserId"));
            User venueUser = userRepository.findById(venueUserId).orElseThrow(() -> new ValidationException(
                    "Venue User with ID %s not found".formatted(venueUserId),
                    ApiErrorCodes.USER_NOT_FOUND));
            addressId = venueUser.getAddress().getAddressId();
        }
        else if(values.get("addressId") != null){
            addressId = UUID.fromString((String) values.get("addressId"));
        }
        else{
            return EventWithTicketOut.builder().ownerUserIds(ownerUserIdsRaw).build();
        }
        AddressOut address = userApi.getAddressOut(addressId.toString());
                
        return EventWithTicketOut.builder()
            .eventId(eventId)
            .locality(address.getLocality())
            .administrativeArea(address.getAdministrativeArea())
            .country(address.getCountry())
            .ownerUserIds(ownerUserIdsRaw).build();
    }

    @DgsData(parentType = "EventWithTicketOut", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForEventWithTicketOut(DgsDataFetchingEnvironment dfe) {
        EventWithTicketOut eventWithTicketOut = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(UserSummaryOutListLoader.class);
        return fetchOrganizers(eventWithTicketOut.getOwnerUserIds(), dataLoader);
    }

    //getEvents, getUserIncludedEvents, searchUserIncludedEvents, searchEvent query
    @DgsEntityFetcher(name = "EventSummaryOut")
    public EventSummaryOut resolveEventSummaryOut(Map<String, Object> values) {
        values.forEach((key, value) -> {
            logger.info("Key: {}, Value: {}", key, value);
        });


        String ownerUserIdsRaw = (String) values.get("ownerUserIds");
        String eventId = (String) values.get("eventId");

        List<String> ownerUserIdsStr = new ArrayList<>();

        if (ownerUserIdsRaw != null && !ownerUserIdsRaw.isBlank()) {
            ownerUserIdsStr = Arrays.asList(ownerUserIdsRaw.split(","));
        }

        if (ownerUserIdsStr.isEmpty()) {
            throw new IllegalArgumentException("ownerUserIdValues are missing or null");
        }

        UUID addressId;
        if(values.get("venueUserId") != null) {
            UUID venueUserId = UUID.fromString((String) values.get("venueUserId"));
            User venueUser = userRepository.findById(venueUserId).orElseThrow(() -> new ValidationException(
                    "Venue User with ID %s not found".formatted(venueUserId),
                    ApiErrorCodes.USER_NOT_FOUND));
            addressId = venueUser.getAddress().getAddressId();
        }
        else if(values.get("addressId") != null){
            addressId = UUID.fromString((String) values.get("addressId"));
        }
        else{
            return EventSummaryOut.builder().ownerUserIds(ownerUserIdsRaw).build();
        }
        AddressOut address = userApi.getAddressOut(addressId.toString());

        return EventSummaryOut.builder()
                .eventId(eventId)
                .locality(address.getLocality())
                .administrativeArea(address.getAdministrativeArea())
                .country(address.getCountry())
                .ownerUserIds(ownerUserIdsRaw).build();
    }

    @DgsData(parentType = "EventSummaryOut", field = "organizers")
    public CompletableFuture<List<UserSummaryOut>> organizersForEventSummaryOut(DgsDataFetchingEnvironment dfe) {
        EventSummaryOut eventSummaryOut = dfe.getSourceOrThrow();
        DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader = dfe.getDataLoader(UserSummaryOutListLoader.class);
        return fetchOrganizers(eventSummaryOut.getOwnerUserIds(), dataLoader);
    }


    private String getExtUserId() {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth == null) {
            throw new RuntimeException("Authentication is null");
        }

        JwtAuthenticationToken jwtAuth = (JwtAuthenticationToken) auth;

        if (jwtAuth.getToken() == null) {
            throw new RuntimeException("JWT Token is null");
        }

        Jwt jwt = jwtAuth.getToken();
        logger.debug("subject: {}", jwt.getSubject());
        String extUserId = userHelper.getExtUserId(jwt.getSubject());
        logger.info("extUserId: " + extUserId);
        if (extUserId == null) {
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Invalid token");
        }
        return extUserId;
    }

    private CompletableFuture<UserSummaryOut> fetchEditor(String ownerUserId, DataLoader<UUID, UserSummaryOut> dataLoader) {
        if (ownerUserId == null) {
            return CompletableFuture.completedFuture(null);
        }
        UUID ownerUuid = UUID.fromString(ownerUserId);
        logger.debug("Fetching organizedBy ownerUserId={}", ownerUuid);
        return dataLoader.load(ownerUuid);
    }

    private CompletableFuture<VenueUserSummaryOut> fetchVenue(String venueUserId, DataLoader<UUID, VenueUserSummaryOut> dataLoader) {
        if (venueUserId == null) {
            return CompletableFuture.completedFuture(null);
        }
        UUID venueUuid = UUID.fromString(venueUserId);
        logger.debug("Fetching venue venueUserId={}", venueUuid);
        return dataLoader.load(venueUuid);
    }

    private CompletableFuture<List<UserSummaryOut>> fetchArtists(String artistIds, DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader) {
        if (artistIds == null || artistIds.isEmpty()) {
            return CompletableFuture.completedFuture(Collections.emptyList());
        }
        List<String> artistIdsStr = Arrays.asList((artistIds).split(","));
        List<UUID> artistIdList = artistIdsStr.stream().map(UUID::fromString).toList();
        logger.debug("artists artistIdList={}", artistIdList);
        return dataLoader.load(artistIdList);
    }

    private CompletableFuture<List<UserSummaryOut>> fetchOrganizers(String organizerIds, DataLoader<List<UUID>, List<UserSummaryOut>> dataLoader) {
        if (organizerIds == null || organizerIds.isEmpty()) {
            return CompletableFuture.completedFuture(Collections.emptyList());
        }
        List<String> organizerIdsStr = Arrays.asList((organizerIds).split(","));
        List<UUID> organizerIdList = organizerIdsStr.stream().map(UUID::fromString).toList();
        logger.debug("organizers organizerIdList={}", organizerIdList);
        return dataLoader.load(organizerIdList);
    }

    private CompletableFuture<AddressOut> fetchAddress(String addressId, DataLoader<String, AddressOut> dataLoader) {
        if (addressId == null) {
            return CompletableFuture.completedFuture(null);
        }
        logger.debug("address addressId={}", addressId);
        return dataLoader.load(addressId);
    }
}