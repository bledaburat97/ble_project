import { gql } from "@apollo/client";

/**
 * Small reusable building blocks
 */

export const ADDRESS_FIELDS = gql`
  fragment AddressFields on AddressOut {
    administrativeArea
    country
    description
    fullAddress
    googlePlaceId
    lat
    lng
    locality
  }
`;

export const USER_SUMMARY_FIELDS = gql`
  fragment UserSummaryFields on UserSummaryOut {
    userId
    username
    displayName
    imageUrl
    type
  }
`;

export const USER_SUMMARY_MIN_FIELDS = gql`
  fragment UserSummaryMinFields on UserSummaryOut {
    username
    displayName
    imageUrl
  }
`;

export const VENUE_USER_SUMMARY_FIELDS = gql`
  fragment VenueUserSummaryFields on VenueUserSummaryOut {
    userId
    username
    displayName
    imageUrl
    type
    address { ...AddressFields }
  }
  ${ADDRESS_FIELDS}
`;

export const TICKET_FIELDS = gql`
  fragment TicketFields on TicketOut {
    ticketId
    ticketType
    ticketPrice
    capacity
    remainingCapacity
    saleStartDate
    saleEndDate
    saleEnded
    limitPerUser
    currency { code symbol }
  }
`;

/**
 * Event fragments
 * - Keep them layered: list/card -> basic -> detail
 */

export const EVENT_BASE_FIELDS = gql`
  fragment EventBaseFields on Event {
    eventId
    name
    startDate
    endDate
    imageUrl
    maxImagePixel
    ownerUserIds
    venueUserId
    addressId
    artistIds
  }
`;

export const EVENT_LOCATION_FIELDS = gql`
  fragment EventLocationFields on Event {
    locality
    administrativeArea
    country
  }
`;

export const EVENT_CARD_FIELDS = gql`
  fragment EventCardFields on Event {
    eventId
    name
    startDate
    endDate
    imageUrl
    maxImagePixel
    ownerUserIds
    venueUserId
    addressId
    locality
    administrativeArea
    country
    organizers { ...UserSummaryMinFields }
    hasTicket
  }
  ${USER_SUMMARY_MIN_FIELDS}
`;

/**
 * Full detail (EventDetail page)
 */
export const EVENT_DETAIL_FIELDS = gql`
  fragment EventDetailFields on Event {
    eventId
    name
    description
    startDate
    endDate
    imageUrl
    maxImagePixel
    genres
    otherArtists

    # federation inputs
    addressId
    venueUserId
    ownerUserIds
    artistIds

    organizers { ...UserSummaryFields }
    artists { ...UserSummaryFields }
    venue { ...VenueUserSummaryFields }
    address { ...AddressFields }

    tickets { ...TicketFields }
  }
  ${USER_SUMMARY_FIELDS}
  ${VENUE_USER_SUMMARY_FIELDS}
  ${ADDRESS_FIELDS}
  ${TICKET_FIELDS}
`;


export const EVENT_DASHBOARD_FIELDS = gql`
  fragment EventDashboardFields on EventOutDashboard {
    eventId
    name
    isVisible
    eventStatus
    maxImagePixel

    # federation inputs (ticket/user resolverları için gerekiyorsa)
    ownerUserIds
    venueUserId
    addressId
    artistIds
  }
`;

export const EVENT_DASHBOARD_TICKET_FIELDS = gql`
  fragment EventDashboardTicketFields on TicketOutDashboard {
    ticketId
    ticketType
    ticketPrice
    sellerFee
    serviceFee
    saleEndDateVisible
  }
`;

export const EVENT_WITH_TICKET_FIELDS = gql`
  fragment EventWithTicketFields on EventWithTicketOut {
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
    organizers { ...UserSummaryMinFields }
    ticket {
      ticketPrice
      currency { code symbol }
    }
  }
  ${USER_SUMMARY_MIN_FIELDS}
`;

export const EVENT_ANALYSIS_FIELDS = gql`
  fragment EventAnalysisFields on EventAnalysisOut {
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
      currency { code symbol }
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
`;

export const USER_EVENT_OUT_FIELDS = gql`
  fragment UserEventOutFields on UserEventOut {
    username
    displayName
    userImageUrl
    maxImagePixel
    events { ...EventCardFields }
  }
  ${EVENT_CARD_FIELDS}
`;
