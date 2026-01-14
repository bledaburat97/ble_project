export const GET_PUBLIC_EVENTS = gql`
  query GetPublicEvents(
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 12
    $sort: [EventSort!]
  ) {
    getEvents(scope: PUBLIC, filter: $filter, page: $page, size: $size, sort: $sort) {
      totalCount
      pageInfo { page size hasNextPage }
      nodes {
        eventId
        ownerUserIds
        venueUserId
        addressId
        name
        startDate
        endDate
        imageUrl
        maxImagePixel
        isVisible
        address {
          administrativeArea
          locality
          country
        }
        organizers {
          username
          displayName
          imageUrl
        }
        hasTicket
      }
    }
  }
`;


export const GET_USER_EVENTS = gql`
  query GetUserEvents(
    $scope: EventScope! = OWNED
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 12
    $sort: [EventSort!]
    $search: String
  ) {
    getEvents(scope: $scope, filter: $filter, page: $page, size: $size, sort: $sort, search: $search) {
      totalCount
      pageInfo { page size hasNextPage }
      nodes {
        eventId
        ownerUserIds
        venueUserId
        addressId
        name
        startDate
        endDate
        imageUrl
        maxImagePixel
        isVisible
        address {
          administrativeArea
          locality
          country
        }
        eventStatus
        organizers {
          username
          displayName
          imageUrl
        }
      }
    }
  }
`;

export const GET_ACTIVE_CITIES = gql`
  query GetActiveCities(
    $filter: EventFilter,
    $page: Int = 0,
    $size: Int = 200,
    $sort: [EventSort!]
  ) {
    getEvents(scope: PUBLIC, filter: $filter, page: $page, size: $size, sort: $sort) {
      nodes {
        address {
          locality
          country
        }
      }
    }
  }
`;

export const GET_ACTIVE_EVENTS = gql`
  query GetActiveEvents(
    $filter: EventFilter
    $sort: [EventSort!]
  ) {
    getEvents(scope: PUBLIC, filter: $filter, page: 0, size: 200, sort: $sort) {
      nodes {
        eventId
        ownerUserIds
        venueUserId
        addressId
        address {
          locality
        }
        organizers { 
          userId 
          username 
          displayName
        }
        venue { 
          userId 
          username 
          displayName
        }
      }
    }
  }
`;

export const GET_EVENT = gql`
  query GetEvent($eventId: ID!) {
    getEventById(eventId: $eventId) {
      address {
        administrativeArea
        country
        description
        fullAddress
        googlePlaceId
        lat
        lng
        locality
      }
      addressId
      artistIds
      artists {
        displayName
        imageUrl
        type
        userId
        username
      }
      description
      endDate
      eventId
      genres
      imageUrl
      name
      organizers {
        displayName
        imageUrl
        type
        userId
        username
      }
      otherArtists
      ownerUserIds
      startDate
      tickets {
        capacity
        currency { code symbol }
        limitPerUser
        remainingCapacity
        saleEndDate
        saleEnded
        saleStartDate
        ticketDescription
        ticketId
        ticketPrice
        ticketType
      }
      venue {
        address {
          country
          administrativeArea
          description
          fullAddress
          googlePlaceId
          lat
          lng
          locality
        }
        displayName
        imageUrl
        type
        userId
        username
      }
      venueUserId
      maxImagePixel
    }
  }
`;

export const GET_EVENT_GENRES = gql`
  query GetEventGenres($eventId: ID!) {
    getEventById(eventId: $eventId) {
      eventId
      genres
    }
  }
`;

export const SEARCH_EVENTS = gql`
  query SearchEvents($keyword: String!) {
    getEvents(scope: PUBLIC, search: $keyword, page: 0, size: 12) {
      nodes {
        eventId
        name
      }
    }
  }
`;

/*
//getEventInfo'yu bulamadım

public List<EventInfo> getEventInfoList(List<String> eventIds) {
    String graphqlQuery = """
        query GetEventsByIds($eventIds: [String!]!) {
            getEventsByIds(eventIds: $eventIds) {
                eventId
                name
                startDate
                endDate
                imageUrl
                maxImagePixel
            }
        }
        """;

    Map<String, Object> variables = Map.of("eventIds", eventIds);

    try {
        return graphQLClient.reactiveExecuteQuery(graphqlQuery, variables)
            .map(resp -> resp.extractValueAsObject("getEventsByIds", new TypeRef<List<EventOut>>() {}))
            .map(list -> list.stream()
                .map(e -> new EventInfo(
                    e.getEventId(),
                    e.getName(),
                    e.getStartDate(),
                    e.getEndDate(),
                    e.getImageUrl() // artık imageId değil url dönüyor
                ))
                .toList()
            )
            .block();
    } catch (Exception e) {
        throw new RuntimeException("Failed to get events by ids: ", e);
    }
}
*/