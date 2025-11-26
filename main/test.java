
//Bunun yerine ayrı ayrı event ve modifiedEvent set edilebilir.
private void applyInformationUpdateToEventBase(
        EventBase target,
        UpdateEventInformationIn in,
        List<String> validGenres
) {
    target.setName(in.name());
    target.setDescription(in.description());
    target.setStartDate(in.startDate());
    target.setEndDate(in.endDate());
    target.setEventGenre(eventGenreMapper.dtoToEntity(validGenres, target.getEventGenre()));
}

public interface EventBase {
    void setName(String name);
    void setDescription(String description);
    void setStartDate(Instant startDate);
    void setEndDate(Instant endDate);
    EventGenre getEventGenre();
    void setEventGenre(EventGenre genre);
}

private ModifiedEvent getOrCreateModifiedEvent(Event event) {
    return modifiedEventRepository.findById(event.getEventId())
            .orElseGet(() -> {
                ModifiedEvent me = new ModifiedEvent();
                me.setEventId(event.getEventId());
                me.setName(event.getName());
                me.setDescription(event.getDescription());
                me.setStartDate(event.getStartDate());
                me.setEndDate(event.getEndDate());
                me.setEventGenre(event.getEventGenre());
                me.setOwnerUserIds(event.getOwnerUserIds());
                me.setAddressId(event.getAddressId());
                me.setVenueId(event.getVenueId());
                me.setNonUserArtist(event.getNonUserArtist());
                me.setArtists(event.getArtists());
                me.setCreatedAt(Instant.now());
                me.setUpdatedAt(Instant.now());
                return me;
            });
}


@Override
@Transactional
@Loggable(description = "to update event information")
public void updateEventInformation(String extUserId, UpdateEventInformationIn in) {

    logger.info("Received input: {}", in);
    logger.info("Received extUserId: {}", extUserId);

    if (in.startDate().isBefore(Instant.now())) {
        throw new ValidationException(
                ("This event has already been passed." +
                        "[eventStartDate:%s, currentTime:%s]").formatted(in.startDate(), Instant.now()),
                ApiErrorCodes.EVENT_DATES_NOT_VALID
        );
    }

    if (in.endDate().isBefore(in.startDate())) {
        throw new ValidationException(
                ("Event start date cannot be later than event end date." +
                        "[eventStartDate:%s, eventEndDate:%s]").formatted(in.startDate(), in.endDate()),
                ApiErrorCodes.EVENT_DATES_NOT_VALID
        );
    }

    Long eventIdLong = Tsid.from(in.eventId()).toLong();
    Event event = eventRepository.findById(eventIdLong)
            .orElseThrow(() -> new NotFoundException(
                    "Event is not found. [eventId:%s, extUserId:%s]".formatted(in.eventId(), extUserId)
            ));

    // TODO: owner kontrollü yapıyı ileride ownerUserIds'e taşıyacağız
    if (!event.getOwnerUserExtUserId().equals(extUserId)) {
        throw new ForbiddenException(
                "User is not owner for this event. [extUserId:%s , eventId: %s]".formatted(extUserId, in.eventId())
        );
    }

    List<String> validGenres = genreApi.findValidGenres(in.genres());

    EventStatus status = event.getEventStatus();

    switch (status) {
        case DRAFT -> {
            // Draft ise direkt event üzerinde değiştir
            applyInformationUpdateToEventBase(event, in, validGenres);
            eventRepository.save(event);
        }
        case WAIT_FOR_APPROVAL -> {
            // Onay bekliyorken değiştirilmesin
            throw new ValidationException(
                    "Event is waiting for approval and cannot be modified. [eventId:%s]".formatted(in.eventId()),
                    ApiErrorCodes.EVENT_NOT_EDITABLE
            );
        }
        case APPROVED, MODIFIED -> {
            // APPROVED ise -> modified_event yarat/güncelle + status -> MODIFIED
            // MODIFIED ise -> mevcut modified_event satırını güncelle

            ModifiedEvent modifiedEvent = getOrCreateModifiedEvent(event);
            applyInformationUpdateToEventBase(modifiedEvent, in, validGenres);
            modifiedEvent.setUpdatedAt(Instant.now());
            modifiedEventRepository.save(modifiedEvent);

            if (status == EventStatus.APPROVED) {
                event.setEventStatus(EventStatus.MODIFIED);
                eventRepository.save(event);
            }
        }
        default -> {
            throw new ValidationException(
                    "Event is not editable in current status. [status:%s]".formatted(status),
                    ApiErrorCodes.EVENT_NOT_EDITABLE
            );
        }
    }
}


private void applyModifiedEventToEvent(ModifiedEvent modified, Event event) {
    event.setName(modified.getName());
    event.setDescription(modified.getDescription());
    event.setStartDate(modified.getStartDate());
    event.setEndDate(modified.getEndDate());
    event.setEventGenre(modified.getEventGenre());
    event.setOwnerUserIds(modified.getOwnerUserIds());
    event.setAddressId(modified.getAddressId());
    event.setVenueId(modified.getVenueId());
    event.setNonUserArtist(modified.getNonUserArtist());
    event.setArtists(modified.getArtists());
    event.setUpdatedAt(Instant.now());
}

@Transactional
public void deleteModifiedEvent(String eventId) {
    Long eventIdLong = Tsid.from(eventId).toLong();

    Event event = eventRepository.findById(eventIdLong)
        .orElseThrow(() -> new NotFoundException(
            "Event is not found. [eventId:%s]".formatted(eventId)
        ));

    if (event.getEventStatus() != EventStatus.MODIFIED) {
        throw new ValidationException(
            "Event is not in MODIFIED status and modification cannot be cancelled. [eventId:%s, status:%s]"
                .formatted(eventId, event.getEventStatus()),
            ApiErrorCodes.EVENT_NOT_EDITABLE
        );
    }

    // İlgili modified_event kaydını sil
    modifiedEventRepository.deleteById(eventIdLong);

    // Event tekrar approved hale dönsün
    event.setEventStatus(EventStatus.APPROVED);
    eventRepository.save(event);
}

@Transactional
public void approveModifiedEvent(String eventId) {
    Long eventIdLong = Tsid.from(eventId).toLong();

    Event event = eventRepository.findById(eventIdLong)
        .orElseThrow(() -> new NotFoundException(
            "Event is not found. [eventId:%s]".formatted(eventId)
        ));

    if (event.getEventStatus() != EventStatus.MODIFIED) {
        throw new ValidationException(
            "Event is not in MODIFIED status and modification cannot be approved. [eventId:%s, status:%s]"
                .formatted(eventId, event.getEventStatus()),
            ApiErrorCodes.EVENT_NOT_EDITABLE
        );
    }

    ModifiedEvent modifiedEvent = modifiedEventRepository.findById(eventIdLong)
        .orElseThrow(() -> new NotFoundException(
            "Modified event is not found. [eventId:%s]".formatted(eventId)
        ));

    // modified_event içeriğini event'e aktar
    applyModifiedEventToEvent(modifiedEvent, event);

    // Event status tekrar APPROVED olsun
    event.setEventStatus(EventStatus.APPROVED);
    event.setUpdatedAt(Instant.now());
    eventRepository.save(event);

    // İstersen silmek yerine arşiv tablosuna da taşıyabilirsin
    modifiedEventRepository.deleteById(eventIdLong);
}
