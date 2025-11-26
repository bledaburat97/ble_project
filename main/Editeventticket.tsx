import {
    App, Button,
    Checkbox,
    Col,
    Form,
    Input,
    InputNumber,
    Row,
    Select, Typography,
} from "antd";
import BusinessModal from "../common/modals/BusinessModal";
import {useContext, useEffect, useState} from "react";
import {CREATE_TICKET, UPDATE_TICKET} from "derouge-react-common/src/graphql/mutations";
import {GET_ALL_SUPPORTED_CURRENCIES} from "derouge-react-common/src/graphql/queries";
import {useMutation, useQuery} from "derouge-react-common/src/apollo";
import {Currency} from "derouge-react-common/src/graphql/generated/types";
import {dateTimeFormat, dayJsToString, getDayjsNow} from "../../helper/DateHelper.tsx"
import {useTranslation} from "react-i18next";
import {getCustomErrorMessage} from "derouge-react-common/helper/ErrorHelper"
import {BusinessUserContext} from "./BusinessUserProvider.tsx";
import _ from "lodash";
import DrgDatePicker from "../common/forms/DrgDatePicker";
import {
    getValidationRulesForTicketDescription,
    getValidationRulesForTicketType
} from "../../helper/InputValidationHelper.tsx";
import {TICKET_CAPACITY} from "../../../../derouge-react-common/constant/ValidationConstants.tsx";
import {Dayjs} from 'dayjs';
import {TicketOutDashboard} from "derouge-react-common/src/graphql/generated/types";


interface EditEventticket {
    eventId: string;
    ticketId: string | undefined;
    editModalOpen: boolean;
    setEditModalOpen: (open: boolean) => void;
    initialFormValues: Record<string, unknown>;
    onSuccess: (ticket: TicketOutDashboard, operation: string) => void;
    eventStatus: string;
    currentTicket?: TicketOutDashboard;
    otherTicketsHasPositiveCapacity: boolean
}

export default function EditEventTicket({
                                            eventId,
                                            ticketId = undefined,
                                            editModalOpen,
                                            setEditModalOpen,
                                            initialFormValues,
                                            onSuccess,
                                            eventStatus,
                                            currentTicket,
                                            otherTicketsHasPositiveCapacity,
                                        }: EditEventticket) {


    const storedCurrencies = sessionStorage.getItem("currencies");
    const [availableCurrencies, setAvailableCurrencies] = useState(storedCurrencies ? JSON.parse(storedCurrencies) : []);
    const {businessUserInfo} = useContext(BusinessUserContext)

    const [saving, setSaving] = useState(false)
    const [form] = Form.useForm();
    const {message} = App.useApp()
    const {t} = useTranslation();
    const ticketPriceWatch = Form.useWatch('ticketPrice', form);
    const currencyWatch = Form.useWatch('currencyCode', form);
    const [createTicketMutation] = useMutation(CREATE_TICKET);
    const [updateTicketMutation] = useMutation(UPDATE_TICKET);
    
    const isApproved = eventStatus === "APPROVED";
    const isEditingExisting = !!ticketId;

    const originalCapacity =
        isEditingExisting && currentTicket?.capacity != null
            ? currentTicket.capacity
            : undefined;
    const originalRemainingCapacity =
        isEditingExisting && currentTicket?.remainingCapacity != null
            ? currentTicket.remainingCapacity
            : undefined;

    const soldCount =
        isEditingExisting &&
        originalCapacity != null &&
        originalRemainingCapacity != null
            ? originalCapacity - originalRemainingCapacity
            : undefined;

    const isSaleStartLocked =
    isApproved && isEditingExisting && (soldCount ?? 0) > 0;

    const minCapacity =
        isApproved && isEditingExisting && soldCount != null
            ? soldCount
            : TICKET_CAPACITY.MIN;

    const maxCapacity = TICKET_CAPACITY.MAX;

    const originalLimitPerUser =
        isEditingExisting && currentTicket?.limitPerUser != null
            ? currentTicket.limitPerUser
            : undefined;

    // Approved + existing ticket + originalLimit yoksa: limitPerUser değişemez
    const isLimitPerUserLocked =
        isApproved && isEditingExisting && originalLimitPerUser == null;

    // Approved + existing ticket + originalLimit varsa: sadece artırılabilir
    const minLimitPerUser =
        isApproved && isEditingExisting && originalLimitPerUser != null
            ? originalLimitPerUser
            : 1;

    const isFieldLocked = isApproved && isEditingExisting;

    const ticketSaleEndWatch = Form.useWatch("ticketSaleEndDate", form);

    const { data } = useQuery(GET_ALL_SUPPORTED_CURRENCIES, {
        skip: storedCurrencies !== null,
    });


    useEffect(() => {
        if (data && data.getAllSupportedCurrencies) {
            setAvailableCurrencies(data.getAllSupportedCurrencies);
            sessionStorage.setItem("currencies", JSON.stringify(data.getAllSupportedCurrencies));
        }
    }, [data]);


    useEffect(() => {
        if (editModalOpen) {
            form.setFieldsValue(initialFormValues)
        }
    }, [editModalOpen]);

    async function onFinish(inputs: Record<string, unknown>) {
        try {
            setSaving(true)

            const newCapacity = inputs["capacity"] as number | undefined;

            if (
                isApproved &&
                isEditingExisting &&
                soldCount === 0 &&
                newCapacity === 0 &&
                !otherTicketsHasPositiveCapacity
            ) {
                // Backend de ayrıca kontrol etmeli ama burada user-friendly hata veriyoruz
                message.error(
                    t("ticket.capacity_zero_not_allowed_without_other_ticket") 
                    // veya direkt Türkçe:
                    // "Bu bileti sıfırlamadan başka bir bilet yaratmalısınız veya direkt event'i yayından kaldırmalısınız"
                );
                setSaving(false);
                return;
            }

            const updateTicketIn = {
                ticketId: ticketId,
                ticketType: inputs["ticketType"],
                ticketDescription: inputs["ticketDescription"],
                ticketPrice: inputs["ticketPrice"],
                currencyCode: inputs["currencyCode"],
                capacity: inputs["capacity"],
                limitPerUser: inputs["limitPerUser"],
                saleStartDate: dayJsToString(inputs["ticketSaleStartDate"] as Dayjs),
                saleEndDate: dayJsToString(inputs["ticketSaleEndDate"] as Dayjs),
                saleEndDateVisible: inputs["saleEndDateVisible"],
                eventId: undefined

            }

            let response;
            if (ticketId) {
                response = await updateTicketMutation({ variables: { ticketUpdateIn: updateTicketIn } });
                onSuccess(response.data.updateTicket, "UPDATED");
                message.info(t("ticket.updated"));
            } else {
                const createTicketIn = {...updateTicketIn, eventId: eventId}
                response = await createTicketMutation({ variables: { ticketCreateIn: createTicketIn } });
                onSuccess(response.data.createTicket, "CREATED");
                message.info(t("ticket.created"));
            }

            setEditModalOpen(false)
        } catch (e) {
            message.error(t(getCustomErrorMessage(e)))
        } finally {
            setSaving(false)
        }

    }

    function getServiceFee() {
        const matchedRate = businessUserInfo.commissionRate.find(
            (entry: { key: string; value: { percentage: number; fixedAmount: number } }) => entry.key === currencyWatch
        );

        if (!matchedRate || ticketPriceWatch == null) {
            return 0;
        }
        const { percentage, fixedAmount } = matchedRate.value;
        const serviceFee = _.round(
            (ticketPriceWatch * percentage / 100) + fixedAmount,
            2
        );
        return Math.min(serviceFee, ticketPriceWatch);
    }

    function getSellerFee() {
        return _.round(ticketPriceWatch - getServiceFee(), 2)
    }


    return (<BusinessModal title={t("ticket.event_ticket")} open={editModalOpen}
                   width={"50vw"}
                   destroyOnClose={true}
                   footer={[
                       <Button size="small" key="cancel" onClick={() =>
                           setEditModalOpen(false)
                       }>
                           {t("common.cancel")}
                       </Button>,
                       <Button size="small" loading={saving} key="next" type={"primary"}
                               onClick={() => form.submit()}
                       >
                           {t("common.save")}
                       </Button>
                   ]}
                   onCancel={() => setEditModalOpen(false)}
    >
        <Form className="max-w-full"
              preserve={false}
              name="ticketForm"
              initialValues={initialFormValues}
              form={form}
              onFinish={onFinish}
              autoComplete="off"
        >
            <Form.Item
                label={t("ticket.type")}
                name="ticketType"
                rules={getValidationRulesForTicketType()}
            >
                <Input placeholder="Early Birds" disabled={isFieldLocked}/>
            </Form.Item>
            <Form.Item
                label={t("common.description")}
                name="ticketDescription"
                rules={getValidationRulesForTicketDescription()}
            >
                <Input.TextArea disabled={isFieldLocked}/>
            </Form.Item>
            <Row>
                <Col span={8}>
                    <Form.Item
                        name="ticketPrice"
                        tooltip={t("warning.currency_must_be_same")}
                        label={t("ticket.price")}
                        rules={[{
                            required: true,
                        },]}
                    >
                        <InputNumber className="w-full"
                                     min={0}
                                     precision={2}
                                     disabled={isFieldLocked}
                        />
                    </Form.Item>
                </Col>
                <Col span={4}>
                    <Form.Item
                        name="currencyCode"
                        rules={[{
                            required: true,
                        },]}
                    >
                        <Select className="w-full" disabled={isFieldLocked}>
                            {availableCurrencies.map((currency: Currency) => <Select.Option key={currency.code}
                                                                                            value={currency.code}>
                                {currency.symbol === currency.code ? currency.symbol : currency.code + " " + currency.symbol}</Select.Option>)}
                        </Select>
                    </Form.Item>
                </Col>
                {
                    currencyWatch && ticketPriceWatch ?
                        <Col span={12}>
                            <Row>


                                    <Typography.Text  className="ml-5 font-semibold italic"
                                                    >{t("ticket.estimated_fees")}</Typography.Text>

                            </Row>
                            <Row>

                                <Col span={12}>
                                    <Typography.Text  className="ml-5 italic"
                                                     type="secondary">{t("ticket.you_will_get") + ":"}</Typography.Text>
                                    <Typography.Text className="ml-1">{getSellerFee() + " " + currencyWatch}</Typography.Text>
                                </Col>

                                <Col span={12}>
                                    <Typography.Text className={"italic"}
                                                     type="secondary">{t("ticket.service_fee") + ":"}</Typography.Text>
                                    <Typography.Text className="ml-1">{getServiceFee() + " " + currencyWatch}</Typography.Text>

                                </Col>

                            </Row>
                        </Col> : ""}
            </Row>


            <Row>
                <Col span={12}>
                    <Form.Item
                        label={t("ticket.sale_start_date")}
                        name="ticketSaleStartDate"
                        rules={[
                            { required: true },
                            () => ({
                                validator(_, value: Dayjs) {
                                    // Değer yoksa, ya da validation'a gerek olmayan durum
                                    if (!value) return Promise.resolve();

                                    // Eğer sale start kilitliyse (satış olmuşsa), 
                                    // kullanıcı zaten değeri değiştiremiyor. Validation hata vermesin.
                                    if (isSaleStartLocked) {
                                        return Promise.resolve();
                                    }

                                    // Satış yoksa: start, end'i geçemesin
                                    if (ticketSaleEndWatch && value.isAfter(ticketSaleEndWatch)) {
                                        return Promise.reject(
                                            new Error(t("ticket.sale_start_before_end")) // i18n'e ekle
                                        );
                                    }

                                    return Promise.resolve();
                                },
                            }),
                        ]}
                    >
                        <DrgDatePicker
                            showTime={{ format: "HH:mm" }}
                            format={dateTimeFormat}
                            disabled={isSaleStartLocked}   // 🔒 satılmış bilet varsa input kilit
                        />
                    </Form.Item>
                </Col>
                <Col span={12}>
                    <Form.Item
                        label={t("ticket.sale_end_date")}
                        name="ticketSaleEndDate"
                        rules={[
                            { required: true },
                            () => ({
                                validator(_, value: Dayjs) {
                                    if (!value) return Promise.resolve();
                                    if (!isApproved) return Promise.resolve();

                                    const now = getDayjsNow();
                                    if (value.isBefore(now)) {
                                        return Promise.reject(
                                            new Error(t("ticket.sale_end_date_cannot_be_past"))
                                        );
                                    }
                                    return Promise.resolve();
                                },
                            }),
                        ]}
                    >
                        <DrgDatePicker
                            showTime={{ format: "HH:mm" }}
                            format={dateTimeFormat}
                        />
                    </Form.Item>
                </Col>

            </Row>
            <Row>
                <Col span={8}>
                    <Form.Item
                        label={t("ticket.sale_end_date_visible")}
                        tooltip={t("ticket.sale_end_date_visible_description")}
                        name="saleEndDateVisible"
                        valuePropName="checked"
                        rules={[{
                            required: false,
                        },]}
                    >
                        <Checkbox/>
                    </Form.Item>
                </Col>
                <Col span={8}>
                    <Form.Item
                        label={t("ticket.capacity")}
                        name="capacity"
                        rules={[{
                            required: true,
                        },]}
                    >
                        <InputNumber min={minCapacity} max={maxCapacity}/>
                    </Form.Item>
                </Col>
                <Col span={8}>
                    <Form.Item
                        label={t("ticket.limit_per_user")}
                        tooltip={t("ticket.max_buyable_ticket_quantity")}
                        name="limitPerUser"
                        rules={[{
                            required: false,
                        },]}
                    >
                        <InputNumber
                            min={minLimitPerUser}
                            max={35000}
                            disabled={isLimitPerUserLocked}
                        />
                    </Form.Item>
                </Col>
            </Row>
        </Form>

    </BusinessModal>)


}