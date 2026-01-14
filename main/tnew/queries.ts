import { gql } from "@apollo/client";
import {
  EVENT_CARD_FIELDS,
  EVENT_DETAIL_FIELDS,
  EVENT_BASE_FIELDS,
  EVENT_LOCATION_FIELDS,
  USER_SUMMARY_FIELDS,
  USER_SUMMARY_MIN_FIELDS,
  EVENT_DASHBOARD_TICKET_FIELDS,
  EVENT_DASHBOARD_FIELDS,
  EVENT_WITH_TICKET_FIELDS,
  USER_EVENT_OUT_FIELDS,
  EVENT_ANALYSIS_FIELDS
} from "./fragments";


export const PUBLIC_EVENT_CARDS = gql`
  query PublicEventCards(
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 12
    $sort: [EventSort!]
  ) {
    events(scope: PUBLIC, filter: $filter, page: $page, size: $size, sort: $sort) {
      totalCount
      pageInfo { page size hasNextPage }
      nodes { ...EventCardFields }
    }
  }
  ${EVENT_CARD_FIELDS}
`;

export const ACTIVE_CITIES = gql`
  query ActiveCities(
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 200
    $sort: [EventSort!]
  ) {
    events(scope: PUBLIC, filter: $filter, page: $page, size: $size, sort: $sort) {
      nodes { ...EventLocationFields }
    }
  }
  ${EVENT_LOCATION_FIELDS}
`;

export const USER_EVENTS = gql`
  query UserEvents(
    $scope: EventScope! = OWNED
    $filter: EventFilter
    $page: Int = 0
    $size: Int = 12
    $sort: [EventSort!]
    $search: String
  ) {
    events(scope: $scope, filter: $filter, page: $page, size: $size, sort: $sort, search: $search) {
      totalCount
      pageInfo { page size hasNextPage }
      nodes {
        ...EventCardFields
        isVisible
        eventStatus
      }
    }
  }
  ${EVENT_CARD_FIELDS}
`;

export const ACTIVE_EVENTS = gql`
  query ActiveEvents($filter: EventFilter, $sort: [EventSort!]) {
    activeEvents(filter: $filter, sort: $sort) {
      ...EventBaseFields
      ...EventLocationFields
      organizers { ...UserSummaryMinFields }
      venue { userId username displayName }
    }
  }
  ${EVENT_BASE_FIELDS}
  ${EVENT_LOCATION_FIELDS}
  ${USER_SUMMARY_MIN_FIELDS}
`;

export const EVENT_DETAIL = gql`
  query EventDetail($eventId: ID!) {
    event(eventId: $eventId) { ...EventDetailFields }
  }
  ${EVENT_DETAIL_FIELDS}
`;


export const EVENT_GENRES = gql`
  query EventGenres($eventId: ID!) {
    event(eventId: $eventId) {
      eventId
      genres
    }
  }
`;

export const EVENT_DASHBOARD = gql`
  query EventDashboard($eventId: ID!) {
    getSignedInUserEventDetail(eventId: $eventId) {
      ...EventDashboardFields
      tickets {
        ...EventDashboardTicketFields
      }
    }
  }
  ${EVENT_DASHBOARD_FIELDS}
  ${EVENT_DASHBOARD_TICKET_FIELDS}
`;


export const SEARCH_USER_EVENTS_BY_USERNAME = gql`
  query UserEventsByUsername(
    $username: String!
    $size: Int
    $page: Int
    $dateFilterEnum: DateFilterEnum
  ) {
    userEventsByUsername(
      username: $username
      size: $size
      page: $page
      dateFilterEnum: $dateFilterEnum
    ) {
      ...UserEventOutFields
    }
  }
  ${USER_EVENT_OUT_FIELDS}
`;

export const EVENT_ANALYSIS_LIST = gql`
  query EventAnalysisList($from: Instant, $to: Instant, $genres: [String]) {
    eventAnalysisList(from: $from, to: $to, genres: $genres) {
      ...EventAnalysisFields
    }
  }
  ${EVENT_ANALYSIS_FIELDS}
`;

export const SEARCH = gql`
  query Search($keyword: String!) {
    search(keyword: $keyword) {
      keyword
      events { ...EventCardFields }
      artist { ...UserSummaryMinFields }
      organizer { ...UserSummaryMinFields }
      venues { ...UserSummaryMinFields }
    }
  }
  ${EVENT_CARD_FIELDS}
  ${USER_SUMMARY_MIN_FIELDS}
`;

export const UPCOMING_EVENTS_WITH_TICKET_INFO = gql`
  query UpcomingEventsWithTicketInfo {
    getUpcomingEventsWithTicketInfo {
      ...EventWithTicketFields
    }
  }
  ${EVENT_WITH_TICKET_FIELDS}
`;

export const NEWS_RELATED_EVENTS_WITH_TICKET_INFO = gql`
  query NewsRelatedEventsWithTicketInfo($newsId: ID!) {
    getNewsRelatedEventsWithTicketInfo(newsId: $newsId) {
      ...EventWithTicketFields
    }
  }
  ${EVENT_WITH_TICKET_FIELDS}
`;


export const SEARCH_EVENTS = gql`
  query SearchEvents($keyword: String!) {
    searchEvents(keyword: $keyword) {
      eventId
      name
    }
  }
`;




export const GET_PUBLIC_EVENTS = PUBLIC_EVENT_CARDS;
export const GET_ACTIVE_CITIES = ACTIVE_CITIES;
export const GET_ACTIVE_EVENTS = ACTIVE_EVENTS;
export const GET_USER_EVENTS = USER_EVENTS;
export const GET_EVENT = EVENT_DETAIL;
export const GET_EVENT_GENRES = EVENT_GENRES;
export const GET_SIGNED_IN_USER_EVENT_DETAIL = EVENT_DASHBOARD;
