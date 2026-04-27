import {
  SupervisorActivityCreateMessage,
  SupervisorAttachmentCreate,
  SupervisorAttachmentDownloadUrl,
  SupervisorTicketCreate,
  SupervisorTicketDetails,
  SupervisorTicketList,
  type Department,
  type Priority,
  type RequestType,
  type TicketStatus,
} from '@/lib/api'
import { TicketsPage as BaseTicketsPage, type TicketsPageApi } from '@/pages/role/requester/TicketsPage'

const supervisorApi: TicketsPageApi = {
  listTickets: SupervisorTicketList,
  createTicket: SupervisorTicketCreate,
  getDetails: SupervisorTicketDetails,
  createMessage: SupervisorActivityCreateMessage,
  uploadAttachment: SupervisorAttachmentCreate,
  attachmentDownloadUrl: SupervisorAttachmentDownloadUrl,
}

type Props = {
  token: string
  onSignout: () => void
  requestTypes: RequestType[]
  departments: Department[]
  priorities: Priority[]
  ticketStatuses: TicketStatus[]
}

export function TicketsPage(props: Props) {
  return <BaseTicketsPage {...props} api={supervisorApi} />
}
