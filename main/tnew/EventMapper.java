/*

Buradaki tek “bilinmeyen”: eventGenreMapper mapper içine mi yoksa service içine mi koyuyorsun?
Senin eski kodda service içindeydi. Ben burada genres satırını null bıraktım; çünkü EventMapper’a EventGenreMapper eklemeden compile garantisi veremem.
Tercihim: genres mapping’i service’de kalsın ya da mapper’a EventGenreMapper inject edelim. Aşağıda service tarafında çözdüm.

*/

import java.util.List;
import java.util.UUID;

import org.w3c.dom.events.Event;

@Component
public class EventMapper {

    private final StorageHelper storageHelper;
    private static final Logger logger = LoggerFactory.getLogger(EventMapper.class);

    public EventMapper(StorageHelper storageHelper) {
        this.storageHelper = storageHelper;
    }

    public EventAnalysisOut eventSummaryProjectionToEventAnalysisOut(EventSummaryProjection p) {
        String eventIdStr = Tsid.from(p.getEventId()).toString();
        return EventAnalysisOut.builder()
                .eventId(eventIdStr)
                .name(p.getName())
                .startDate(p.getStartDate())
                .endDate(p.getEndDate())
                .imageUrl(storageHelper.getImageUrl(p.getEventImageId()))
                .maxImagePixel(p.getMaxImagePixel())
                .eventStatus(p.getEventStatus())
                .build();
    }

    public EventOut eventSummaryProjectionToEventOut(EventSummaryProjection p) {
        String eventIdStr = Tsid.from(p.getEventId()).toString();

        List<String> ownerIds = (p.getOwnerUserIds() == null)
                ? List.of()
                : p.getOwnerUserIds().stream().map(UUID::toString).toList();

        return EventOut.builder()
                .eventId(eventIdStr)
                .name(p.getName())
                .description(p.getDescription())
                .startDate(p.getStartDate())
                .endDate(p.getEndDate())
                .imageUrl(storageHelper.getImageUrl(p.getEventImageId()))
                .maxImagePixel(p.getMaxImagePixel())
                .ownerUserIds(ownerIds)
                .venueUserId(p.getVenueUserId() == null ? null : p.getVenueUserId().toString())
                .addressId(p.getAddressId() == null ? null : p.getAddressId().toString())
                .isVisible(p.getIsVisible())
                .eventStatus(p.getEventStatus())
                // locality/admin/country projection’da yoksa burada set etme (null kalır)
                .build();
    }

    public EventOut eventToEventOut(Event e) {
        String eventIdStr = Tsid.from(e.getEventId()).toString();

        List<String> ownerIds = (e.getOwnerUserIds() == null)
                ? List.of()
                : e.getOwnerUserIds().stream().map(UUID::toString).toList();

        List<String> artistIds = (e.getArtists() == null)
                ? List.of()
                : e.getArtists().stream().map(UUID::toString).toList();

        List<String> ticketIds = (e.getTicketIds() == null)
                ? null
                : e.getTicketIds().stream().map(UUID::toString).toList();

        return EventOut.builder()
                .eventId(eventIdStr)
                .name(e.getName())
                .description(e.getDescription())
                .startDate(e.getStartDate())
                .endDate(e.getEndDate())
                .imageUrl(storageHelper.getImageUrl(e.getImageId()))
                .maxImagePixel(e.getMaxImagePixel())
                .ownerUserIds(ownerIds)
                .venueUserId(e.getVenueId() == null ? null : e.getVenueId().toString())
                .addressId(e.getAddressId() == null ? null : e.getAddressId().toString())
                .artistIds(artistIds)
                .ticketIds(ticketIds)
                .genres(e.getEventGenre() == null ? null : /* eventGenreMapper.entityToDto(...) */ null)
                .otherArtists(e.getNonUserArtist())
                .isVisible(e.getIsVisible())
                .eventStatus(e.getEventStatus())
                .build();
    }

    public EventOutDashboard eventToEventOutDashboard(Event event, List<String> genres) {
        String eventIdStr = Tsid.from(event.getEventId()).toString();

        return EventOutDashboard.builder()
                .eventId(eventIdStr)
                .name(event.getName())
                .description(event.getDescription())
                .startDate(event.getStartDate())
                .endDate(event.getEndDate())
                .imageUrl(storageHelper.getImageUrl(event.getImageId()))
                .maxImagePixel(event.getMaxImagePixel())
                .eventStatus(event.getEventStatus())
                .isVisible(event.getIsVisible())
                .genres(genres)
                .otherArtists(event.getNonUserArtist())

                // federation inputs
                .ownerUserIds(event.getOwnerUserIds() == null ? List.of()
                        : event.getOwnerUserIds().stream().map(UUID::toString).toList())
                .venueUserId(event.getVenueId() == null ? null : event.getVenueId().toString())
                .addressId(event.getAddressId() == null ? null : event.getAddressId().toString())
                .artistIds(event.getArtists() == null ? null
                        : event.getArtists().stream().map(UUID::toString).toList())
                .build();
        }
}
