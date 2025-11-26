import {useNavigate, useParams} from "react-router-dom";
import '../../styles/Avatar.css';
import {useContext, useEffect, useRef, useState, useCallback} from "react";
import { useQuery , useMutation, useLazyQuery} from "derouge-react-common/src/apollo";
import { GET_SIGNED_IN_USER_EVENT_DETAIL, GET_GUEST_LIST_OF_EVENT } from "derouge-react-common/src/graphql/queries";
import { SEND_EVENT_FOR_APPROVAL, DELETE_EVENT } from "derouge-react-common/src/graphql/mutations";
import {
    UploadControllerApi
} from "derouge-react-common/upload-api-client/index";
import {getCustomErrorMessage} from 'derouge-react-common/helper/ErrorHelper'
import {
    Avatar,
    Button,
    Col,
    Divider,
    Flex,
    Row,
    Skeleton,
    Space,
    Typography,
    Upload,
    Spin,
    Popconfirm, App
} from "antd";
import ImgCrop from "antd-img-crop";
import {USER_TYPE} from "derouge-react-common/constant/UserTypes"
import {validateImage} from "derouge-react-common/helper/ImageHelper";
import {useTranslation} from "react-i18next";
import {BusinessUserContext} from "./BusinessUserProvider.tsx";
import EventTicket from "./EventTicket.tsx";
import EventArtist from "./EventArtist.tsx";
import EventLocation from "./EventLocation.tsx";
import {UploadApiConfiguration} from "../../config/ApiConfiguration.tsx";
import EventInformation from "./EventInformation.tsx";
import {UserContext} from "../UserProvider.tsx";
import ImageView from "../ImageView.tsx";
import SideImageCard from "../common/cards/SideImageCard";
import {CameraOutlined, DeleteOutlined} from "@ant-design/icons";
import { EventOut, EventOutDashboard, TicketOutDashboard} from "derouge-react-common/src/graphql/generated/types";

export default function EventDetailDashboard() {
    const {eventId} = useParams()
    const uploadControllerApi = new UploadControllerApi(UploadApiConfiguration)
    const {getAuthHeader} = useContext(UserContext)
    const navigate = useNavigate()
    const {message} = App.useApp()
    const {businessUserInfo} = useContext(BusinessUserContext)
    const uploadRef = useRef<HTMLDivElement>(null)
    const [uploadingImage, setUploadingImage] = useState(false)
    const [sending, setSending] = useState(false)
    const [deleting, setDeleting] = useState(false)

    const {t} = useTranslation();
    const [sendEventForApprovalMutation] = useMutation(SEND_EVENT_FOR_APPROVAL);
    const [deleteEventMutation] = useMutation(DELETE_EVENT);
    const [getGuestListOfEventQuery] = useLazyQuery(GET_GUEST_LIST_OF_EVENT);

    const { data, loading , refetch} = useQuery(GET_SIGNED_IN_USER_EVENT_DETAIL, {
        variables: {
            eventId: eventId || "",
        },
        onCompleted: () => {
        },
        onError: (err) => {
            message.error(t(getCustomErrorMessage(err)));
            navigate('/');
        },
    });

    const [event, setEvent] = useState(data?.getSignedInUserEventDetail || null);

    const fetchEventDetail = useCallback(async () => {
        if (!eventId) return;

        try {
            const { data: refreshedData } = await refetch();    
            if (refreshedData?.getSignedInUserEventDetail) {
                setEvent(refreshedData.getSignedInUserEventDetail);
            } else {
            }
        } catch (e) {
        }
    }, [eventId, refetch]);

    useEffect(() => {
        fetchEventDetail();
    }, [eventId]);
    
      if (loading) {
        return <Skeleton active />;
      }
    
      if (!event) {
        return <div>{t('event.not_found')}</div>;
      }

    async function downloadCSVFile(eventId: string) {
        try {
            const { data } = await getGuestListOfEventQuery({ variables: { eventId } });
    
            if (data && data.getGuestListOfEvent) {
                const { content, fileName } = data.getGuestListOfEvent;
    
                if (content && fileName) {
                    const decodedContent = atob(content);
                    const blob = new Blob([decodedContent], { type: "text/csv;charset=utf-8;" });
                    const url = window.URL.createObjectURL(blob);
    
                    const link = document.createElement("a");
                    link.href = url;
                    link.setAttribute("download", fileName);
                    document.body.appendChild(link);
                    link.click();
    
                    window.URL.revokeObjectURL(url);
                    document.body.removeChild(link);
                } else {
                }
            } else {
            }
        } catch (error) {
        }
    }

    const isOwner = event.organizers.some(o => o.username === businessUserInfo.username);
    const isDraft = event?.eventStatus === "DRAFT";
    const isWaitForApproval = event?.eventStatus === "WAIT_FOR_APPROVAL";
    const isApproved = event?.eventStatus === "APPROVED";
    const isModified = event?.eventStatus === "MODIFIED";
    const hasUnapprovedChanges = event?.hasUnapprovedChanges === true;

    function isEventEditable() {
        if (!isOwner) return false;

        // İçerik alanları (artist, organizer, info, location...) DRAFT ve APPROVED iken editleyebilir
        if (isDraft || isApproved) {
            return true;
        }

        // WAIT_FOR_APPROVAL ve MODIFIED daima kilitli
        return false;
    }

    function isEventApproved() {
        return (event?.eventStatus === "APPROVED" && event.organizedBy?.username === businessUserInfo.username)
    }

    function isLocationEditable() {
        return (businessUserInfo.type !== USER_TYPE.VENUE && isEventEditable())
    }


    async function uploadFile(eventId: any, upload: any) {

        try {
            setUploadingImage(true)
            const validated = validateImage(upload.file);
            if (!validated) {
                message.error(t("warning.upload_image_type_size"));
                return;
            }
            const response = await uploadControllerApi.uploadEventImage(eventId, upload.file, await getAuthHeader())
            setEvent({...event, imageUrl: response.data})
        } catch (err) {
            message.error(t(getCustomErrorMessage(err)));
        } finally {
            setUploadingImage(false)
        }
    }

    function hasTicketsSameCurrency(tickets: Array<TicketOutDashboard> | undefined) {
        if (tickets && tickets.length > 1) {
            const currency = tickets[0].currency
            const anyDifferentCurrency = tickets.find(ticket => ticket.currency?.code !== currency?.code)
            if (anyDifferentCurrency) {
                return false;
            }
        }
        return true
    }

    async function sendForApproval(eventId: string) {
        try {
            setSending(true);

            if (!hasTicketsSameCurrency(event?.tickets)) {
                message.error(t("warning.currency_must_be_same"));
                return;
            }

            await sendEventForApprovalMutation({
                variables: {
                    eventId: eventId,
                },
            });

            message.info(t("messages.event_send_success"));
            setEvent({ ...event, eventStatus: "WAIT_FOR_APPROVAL" });
        } catch (e) {
            message.error(t(getCustomErrorMessage(e)));
        } finally {
            setSending(false);
        }
    }

    async function deleteEvent(eventId: string) {
        try {
            setDeleting(true);

            await deleteEventMutation({
                variables: {
                    eventId: eventId,
                },
            });

            message.info(t("messages.delete_event_success"));
            navigate("/pro/dashboard/my-events");
        } catch (e) {
            message.error(t(getCustomErrorMessage(e)));
        } finally {
            setDeleting(false);
        }
    }

    async function onEventUpdateSuccess() {
        await fetchEventDetail();
    }

    async function onTicketChangeSuccess(value: any, operation: string) {
        let newTickets;
        switch (operation) {
            case "DELETED":
                newTickets = event?.tickets?.filter((ticket: TicketOutDashboard | null | undefined) => ticket?.ticketId !== value);
                break;
            case "UPDATED":
                newTickets = event?.tickets?.map((ticket: TicketOutDashboard | null | undefined) => {
                    if (ticket?.ticketId === value.ticketId) {
                        return value;
                    }
                    return ticket;
                })
                break;

            case "CREATED":
                if (event?.tickets) {
                    newTickets = [...event.tickets, value]
                }
                break;

            default:
                break;
        }

        if (newTickets) {
            setEvent({...event, tickets: newTickets})
        }
    }

    return <>
        {loading ? <Skeleton> </Skeleton> :
            <>
                <Row>
                    <Col span={24} className={"text-right"}>
                        {isOwner && (
                        <Space>
                            {/* DRAFT iken: delete + send for approval */}
                            {isDraft && (
                            <>
                                <Popconfirm
                                title={t("event.delete")}
                                onConfirm={() => deleteEvent(eventId as string)}
                                description={t("warning.delete_pop_confirm")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                                >
                                <Button icon={<DeleteOutlined />} loading={deleting} disabled={deleting} />
                                </Popconfirm>

                                <Popconfirm
                                title={t("event.send_for_approval")}
                                onConfirm={() => sendForApproval(eventId as string)}
                                description={t("warning.send_for_approval_popconfirm")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                                >
                                <Button loading={sending} disabled={sending}>
                                    {t("event.send_for_approval")}
                                </Button>
                                </Popconfirm>
                            </>
                            )}

                            {/* WAIT_FOR_APPROVAL iken: cancel waiting for approval */}
                            {isWaitForApproval && (
                            <Popconfirm
                                title={t("event.cancel_wait_for_approval")}
                                onConfirm={() => cancelWaitForApproval(eventId as string)} // yeni mutation
                                description={t("warning.cancel_wait_for_approval")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                            >
                                <Button>{t("event.cancel_wait_for_approval")}</Button>
                            </Popconfirm>
                            )}

                            {/* APPROVED + has_unapproved_changes = true: update + delete changes */}
                            {isApproved && hasUnapprovedChanges && (
                            <>
                                <Popconfirm
                                title={t("event.update_approved_event")}
                                onConfirm={() => updateApprovedEvent(eventId as string)} // event status -> MODIFIED
                                description={t("warning.update_approved_event")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                                >
                                <Button>{t("event.update_approved_event")}</Button>
                                </Popconfirm>

                                <Popconfirm
                                title={t("event.delete_changes")}
                                onConfirm={() => deleteChanges(eventId as string)} // modified_event sil
                                description={t("warning.delete_changes")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                                >
                                <Button>{t("event.delete_changes")}</Button>
                                </Popconfirm>
                            </>
                            )}

                            {/* MODIFIED iken: cancel modification approvement */}
                            {isModified && (
                            <Popconfirm
                                title={t("event.cancel_modification_approvement")}
                                onConfirm={() => cancelModificationApprovement(eventId as string)} // status -> APPROVED
                                description={t("warning.cancel_modification_approvement")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                            >
                                <Button>{t("event.cancel_modification_approvement")}</Button>
                            </Popconfirm>
                            )}

                            {/* Guest list: approved/modifed durumlarında (istersen sadece approved yap) */}
                            {isEventApprovedForGuestList() && (
                            <Popconfirm
                                title={t("event.get_guest_list")}
                                onConfirm={() => downloadCSVFile(eventId as string)}
                                description={t("warning.get_guest_list")}
                                okText={t("common.yes")}
                                cancelText={t("common.no")}
                            >
                                <Button>{t("event.get_guest_list")}</Button>
                            </Popconfirm>
                            )}
                        </Space>
                        )}
                    </Col>
                </Row>
                <Divider/>
                <div style={{maxWidth: '1280px', margin: '0 auto'}}>
                    <Row gutter={[32, 32]}>
                        <Col xs={{span: 24, order: 2}} md={{span: 16, order: 2}}>
                            <Flex vertical>
                                <Flex justify="space-between">
                                    <Typography.Title level={3}>{event?.name}</Typography.Title>
                                    <Avatar
                                        className={"avatar-image"} icon={
                                        <ImageView imageUrl={event.organizers?.[0]?.imageUrl} size={"sm"}/>
                                    }/>
                                </Flex>
                                <EventInformation event={event as EventOut} onSuccess={onEventUpdateSuccess}
                                                  isEditable={isEventEditable()}/>
                                <EventLocation event={event as EventOut} onSuccess={onEventUpdateSuccess}
                                               isEditable={isLocationEditable()}/>
                                <EventArtist event={event as EventOut} onSuccess={onEventUpdateSuccess}
                                             isEditable={isEventEditable()}/>
                            </Flex>
                        </Col>
                        <Col xs={{span: 24, order: 1}} md={{span: 8, order: 1}}>
                            {uploadingImage ? <Spin/> : (
                                <SideImageCard
                                    imageUrl={event?.imageUrl}
                                    title={t("ticket.tickets")}
                                    icon={isEventEditable() ? <CameraOutlined /> : undefined}
                                    onImageClick={isEventEditable() ? () => { uploadRef?.current?.click(); } : undefined}
                                    maxImageSize={event.maxImagePixel}
                                >
                                    <div style={{ padding: 24 }}>
                                        <EventTicket event={event as EventOutDashboard}
                                                    tickets={event?.tickets as Array<TicketOutDashboard>}
                                                    onSuccess={onTicketChangeSuccess}
                                                    isEditable={isEventEditable()}/>
                                    </div>
                                </SideImageCard>
                            )}
                            <ImgCrop aspect={1} modalOk="Save" modalCancel="Cancel">
                                <Upload
                                    className="mt-5"
                                    maxCount={1}
                                    showUploadList={false}
                                    customRequest={(upload) => uploadFile(event?.eventId, upload)}
                                >
                                    <div ref={uploadRef}/>
                                </Upload>
                            </ImgCrop>
                        </Col>
                    </Row>
                </div>
            </>}


    </>

}