import { gql } from "@apollo/client";

export const GET_PUBLIC_EVENTS = gql`
  query GetPublicEvents(
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 12
    $sort: [EventSort!]
  ) {
    getPublicEvents(filter: $filter, page: $page, size: $size, sort: $sort) {
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
        administrativeArea
        locality
        country
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

export const GET_ACTIVE_CITIES = gql`
  query GetActiveCities($filter: EventFilter, $page: Int = 0, $size: Int = 200, $sort: [EventSort!]) {
    getPublicEvents(filter: $filter, page: $page, size: $size, sort: $sort) {
      nodes {
        locality
        country
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
    getUserEvents(scope: $scope, filter: $filter, page: $page, size: $size, sort: $sort, search: $search) {
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
        administrativeArea
        locality
        country
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


export const GET_ACTIVE_EVENTS = gql`
  query GetActiveEvents(
    $filter: EventFilter
    $sort: [EventSort!]
  ) {
    getActiveEvents(filter: $filter, sort: $sort) {
      eventId
      ownerUserIds
      venueUserId
      addressId
      locality
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
`;


export const GET_EVENT = gql`
query GetEvent($eventId: ID!) {
  getEvent(eventId: $eventId) {
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
      currency {
        code
        symbol
      }
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
    getEvent(eventId: $eventId) {
      eventId
      genres
    }
  }
`;


export const SEARCH_EVENTS = gql`
  query SearchEvents($keyword: String!) {
    searchEvents(keyword: $keyword) {
      eventId
      name
    }
  }
`;

export const GET_SIGNED_IN_USER_EVENT_DETAIL = gql`
query GetSignedInUserEventDetail($eventId: ID!) {
    getSignedInUserEventDetail(eventId: $eventId) {
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
      eventStatus
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
        currency {
          code
          symbol
        }
        limitPerUser
        remainingCapacity
        saleEndDate
        saleEndDateVisible
        saleStartDate
        sellerFee
        serviceFee
        ticketDescription
        ticketId
        ticketPrice
        ticketType
      }
      venue {
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
        displayName
        imageUrl
        type
        userId
        username
      }
      venueUserId
      maxImagePixel
      isVisible
    }
  }
`;

export const SEARCH_USER_EVENTS_BY_USERNAME= gql`
query SearchUserEventsByUsername($username: String!, $size: Int, $page: Int, $dateFilterEnum: DateFilterEnum) {
    searchUserEventsByUsername(username: $username, size: $size, page: $page, dateFilterEnum: $dateFilterEnum) {
      displayName
      events {
        addressId
        administrativeArea
        country
        description
        endDate
        eventId
        imageUrl
        isVisible
        locality
        name
        ownerUserIds
        startDate
        venueUserId
        organizers {
          userId
          username
          displayName
          imageUrl
          type
        }
      }
      userImageUrl
      username
      maxImagePixel
    }
  }
`;

export const GET_EVENT_ANALYSIS_LIST = gql`
  query GetEventAnalysisList($from: Instant, $to: Instant, $genres: [String]) {
    getEventAnalysisList(from: $from, to: $to, genres: $genres) {
      eventId
      name
      startDate
      endDate
      imageUrl
      maxImagePixel
      eventStatus
      filterFrom
      filterTo
      soldTickets {
        ticketId
        ticketType
        ticketPrice
        sellerFee
        serviceFee
        currency {
          code
          symbol
        }
        capacity
        remainingCapacity
        filterFrom
        filterTo
        bookedTickets {
          boughtTime
          count
        }
      }
    }
  }
`;

export const SEARCH = gql`
  query Search($keyword: String!) {
    search(keyword: $keyword) {
      keyword
      events {
        addressId
        administrativeArea
        country
        description
        endDate
        eventId
        imageUrl
        isVisible
        locality
        name
        ownerUserIds
        startDate
        venueUserId
        organizers {
          userId
          username
          displayName
          imageUrl
          type
        }
      }
      artist {
        userId
        username
        displayName
        imageUrl
        type
      }
      organizer {
        userId
        username
        displayName
        imageUrl
        type
      }
      venues {
        userId
        username
        displayName
        imageUrl
        type
      }
    }
  }
`;


export const GET_UPCOMING_EVENTS_WITH_TICKET_INFO = gql`
  query GetUpcomingEventsWithTicketInfo {
    getUpcomingEventsWithTicketInfo {
      eventId
      name
      startDate
      endDate
      imageUrl
      maxImagePixel
      venueUserId
      addressId
      locality
      administrativeArea
      country
      ticket {
        ticketPrice
        currency {
          code
          symbol
        }
      }
      organizers {
        username
        displayName
      }
    }
  }
`;

export const GET_NEWS_RELATED_EVENTS_WITH_TICKET_INFO = gql`
  query GetNewsRelatedEventsWithTicketInfo($newsId: ID!) {
    getNewsRelatedEventsWithTicketInfo(newsId: $newsId) {
      eventId
      name
      startDate
      endDate
      imageUrl
      maxImagePixel
      venueUserId
      addressId
      locality
      administrativeArea
      country
      ticket {
        ticketPrice
        currency {
          code
          symbol
        }
      }
      organizers {
        username
        displayName
      }
    }
  }
`;