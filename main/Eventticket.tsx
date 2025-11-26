import {App, Button, Divider, Flex, Popconfirm, Space, Typography} from "antd";
import OptionCard from "../common/cards/OptionCard";
import EditEventTicket from "./EditEventTicket.jsx";
import {DeleteOutlined, EditOutlined, PlusOutlined} from "@ant-design/icons";
import {useRef, useState} from "react";
import {
    getDayjs, getDayjsNow,
    isoStringToFormattedString
} from "../../helper/DateHelper.tsx";
import {DELETE_TICKET} from "derouge-react-common/src/graphql/mutations";
import {useMutation} from "derouge-react-common/src/apollo";
import { EventOutDashboard, TicketOutDashboard} from "derouge-react-common/src/graphql/generated/types";
import {useTranslation} from "react-i18next";
import {getSomethingWentWrongErrorMessage} from "derouge-react-common/constant/ErrorCodes"

interface EventTicketProps {
    event: EventOutDashboard;
    tickets: Array<TicketOutDashboard>;
    onSuccess: (ticket: TicketOutDashboard | string, operation: string) => void;
    isEditable: boolean;
}

export default function EventTicket({event, tickets, onSuccess, isEditable}: EventTicketProps) {

    const [editModalOpen, setEditModalOpen] = useState(false)
    const ticketIndex = useRef<number | undefined>()
    const {message} = App.useApp()
    const {t} = useTranslation();

    const [deleteTicketMutation] = useMutation(DELETE_TICKET);

    const isApproved = event.eventStatus === "APPROVED";
    const isDraft = event.eventStatus === "DRAFT";
    const [otherTicketsHasPositiveCapacity, setOtherTicketsHasPositiveCapacity] = useState<boolean>(false);

    async function handleDeleteTicket(ticketId: string) {
        try {
            const ticketDeleteIn = { ticketId };
            await deleteTicketMutation({ variables: { ticketDeleteIn: ticketDeleteIn } });
            onSuccess(ticketId, "DELETED")
            message.info(t("ticket.deleted"))
        } catch (e) {
            message.error(t(getSomethingWentWrongErrorMessage()))        }
    }


    return (
        <>
            {isEditable && <EditEventTicket editModalOpen={editModalOpen} setEditModalOpen={setEditModalOpen}
                                            ticketId={ticketIndex.current ? tickets[ticketIndex.current]?.ticketId : undefined}
                                            eventId={event.eventId as string}
                                            eventStatus={event.eventStatus as string}
                                            currentTicket={
                                                ticketIndex.current != null &&
                                                tickets?.length > ticketIndex.current
                                                    ? tickets[ticketIndex.current]
                                                    : undefined
                                            }
                                            otherTicketsHasPositiveCapacity={otherTicketsHasPositiveCapacity}
                                            onSuccess={onSuccess}
                                            initialFormValues={ticketIndex.current != null  && tickets?.length > ticketIndex.current ? {
                                                    ticketType: tickets[ticketIndex.current].ticketType,
                
                                                    ticketDescription: tickets[ticketIndex.current].ticketDescription,
                                                    ticketPrice: tickets[ticketIndex.current].ticketPrice,
                                                    currencyCode: tickets[ticketIndex.current].currency?.code,
                                                    limitPerUser: tickets[ticketIndex.current].limitPerUser,
                                                    ticketSaleStartDate: getDayjs(tickets[ticketIndex.current].saleStartDate as string),
                                                    ticketSaleEndDate: getDayjs(tickets[ticketIndex.current].saleEndDate as string),
                                                    capacity: tickets[ticketIndex.current].capacity,
                                                    saleEndDateVisible: tickets[ticketIndex.current].saleEndDateVisible
                                                } :
                                                {
                                                    ticketPrice: "0.00",
                                                    ticketSaleStartDate: getDayjsNow().minute(0).second(0).add(1, 'h'),
                                                    ticketSaleEndDate: getDayjsNow().minute(0).second(0).add(1, 'h').add(1, 'd')
                                                }}
            ></EditEventTicket>}
            <Space direction="vertical" style={{ width: '100%' }}>
                {tickets?.length > 0 && tickets.map((ticket, index) => (
                    <div key={index}>
                        <OptionCard
                            bordered={false}
                            style={{ backgroundColor: '#fff', color: '#000' }}
                            title={
                                <Flex justify="space-between" align="center" style={{ width: '100%', marginLeft: '0px' }}>
                                    <Typography.Text style={{ color: '#000', fontWeight: 'bold' }}>
                                        {ticket.ticketType}
                                    </Typography.Text>
                                    <Typography.Text style={{ color: '#000', fontWeight: 'bold', marginRight: isEditable ? '8px' : '0px' }}>
                                        {ticket.ticketPrice === 0 ? t("ticket.free") : `${ticket.ticketPrice} ${ticket.currency?.code}`}
                                    </Typography.Text>
                                </Flex>
                            }
                            extra={isEditable && (
                                <div style={{ marginRight: '0px' }}>
                                    <Space>
                                        <Button size="small" onClick={() => {
                                            ticketIndex.current = index;
                                            const hasOther = tickets.some(
                                                (t, i) => i !== index && (t.capacity ?? 0) > 0
                                            );
                                            setOtherTicketsHasPositiveCapacity(hasOther);
                                            setEditModalOpen(true)
                                        }} icon={<EditOutlined/>}/>
                                        {!isApproved && (
                                            <Popconfirm
                                                title={t("ticket.delete")}
                                                onConfirm={() => handleDeleteTicket(ticket.ticketId as string)}
                                                description={t("common.are_you_sure")}
                                                okText={t("common.yes")}
                                                cancelText={t("common.no")}
                                            >
                                                <Button size="small" danger icon={<DeleteOutlined/>}/>
                                            </Popconfirm>
                                        )}
                                    </Space>
                                </div>
                            )}
                        >
                            {ticket.ticketDescription && (
                                <Typography.Paragraph className="mb-0" style={{ color: '#000', marginBottom: '8px' }}>
                                    {ticket.ticketDescription}
                                </Typography.Paragraph>
                            )}
                            <Typography.Text type="secondary" style={{ color: '#666' }}>
                                {t("ticket.sale_start_date")}: {isoStringToFormattedString(ticket?.saleStartDate ? ticket.saleStartDate : "")}
                            </Typography.Text><br/>
                            <Typography.Text type="secondary" style={{ color: '#666' }}>
                                {t("ticket.sale_end_date")}: {isoStringToFormattedString(ticket?.saleEndDate ? ticket.saleEndDate : "")}
                            </Typography.Text><br/>
                            <Typography.Text type="secondary" style={{ color: '#666' }}>
                                {t("ticket.capacity")}: {ticket.remainingCapacity} / {ticket.capacity}
                            </Typography.Text><br/>
                            <Typography.Text type="secondary" style={{ color: '#666' }}>
                                {t("ticket.limit_per_user")}: {ticket.limitPerUser ? ticket.limitPerUser : t("ticket.no_limit")}
                            </Typography.Text>
                        </OptionCard>
                        {index !== tickets.length - 1 && <Divider style={{ margin: '4px 0' }} />}
                    </div>
                ))}
                {isEditable && (
                    <Button
                        className="sidecard-light-button"
                        icon={<PlusOutlined/>}
                        onClick={() => {
                            ticketIndex.current = undefined
                            setEditModalOpen(true);
                        }}
                        block
                    >
                        {t("ticket.add_new")}
                    </Button>
                )}
            </Space>
        </>
    )
}