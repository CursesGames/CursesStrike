#include "bcsstatemachine.h"
#include "../liblinux_util/linux_util.h"

bool bcsstatemachine_process_request(
	  BCSSERVER_FULL_STATE *state
	, sockaddr_in *src
	, BCSMSG *msg
	, ssize_t msglen
) {
	return false;

	/*
    // why the client may be denied
    id = search_client(clients, addr_client);
    serv_msg.packet_no = cl_msg.packet_no; // packet to send number

    switch(be32toh(cl_msg.action)){
        case BCSACTION_CONNECT: // client sent CONNECT
            // Ignore beacon packets
            if(result >= 8 && be64toh(*((uint64_t*)&cl_msg)) == BCSBEACON_MAGIC)
                continue;
            printf("received CONNECT from client\n");
            // Add client to array
            switch(add_client(&map, clients, addr_client)){
                case -1: // clients limit is settled
                    serv_msg.type = htobe32(BCSREPLT_NACK);
                    break;

                default:
                    clients[id].public_info.state = BCSCLST_CONNECTING; //client state to CONNECTING
                    serv_msg.type = htobe32(BCSREPLT_MAP);
                    log_print_cl_info(clients);
            }
            break;
            
        case BCSACTION_CONNECT2: // client sent CONNECT2
            printf("received CONNECT2 from client\n");
            // Change client stat if the previous was BCSCLST_CONNECTING
            if(clients[id].public_info.state == BCSCLST_CONNECTING){
                clients[id].public_info.state = BCSCLST_CONNECTED;
                serv_msg.type = htobe32(BCSREPLT_ACK);
            }
            else{
                //error
                clients[id].public_info.state = BCSCLST_UNDEF;
                serv_msg.type = htobe32(BCSREPLT_NACK);
            }
            break;
                
        case BCSACTION_DISCONNECT:
            delete_client(clients, addr_client);
            serv_msg.type = htobe32(BCSREPLT_ACK);
            break;

        case BCSACTION_MOVE:
            switch(be32toh(cl_msg.un.ints.int_lo)){
                case BCSDIR_LEFT:
                    if((clients[id].public_info.position.x - 1) == 0){
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                    }
                    else{
                        clients[id].public_info.position.x--;
                        serv_msg.type = htobe32(BCSREPLT_ACK);
                    }
                    break;
                        
                case BCSDIR_RIGHT:
                    if((clients[id].public_info.position.x + 1) == map.width){
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                    }
                    else{
                        clients[id].public_info.position.x++;
                        serv_msg.type = htobe32(BCSREPLT_ACK);
                    }
                    break;

                case BCSDIR_UP:
                    if((clients[id].public_info.position.y - 1) == 0){
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                    }
                    else{
                        clients[id].public_info.position.y--;
                        serv_msg.type = htobe32(BCSREPLT_ACK);
                    }
                    break;

                case BCSDIR_DOWN:
                    if((clients[id].public_info.position.y + 1) == map.height){
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                    }
                    else{
                        clients[id].public_info.position.y++;
                        serv_msg.type = htobe32(BCSREPLT_ACK);
                    }

                    default:
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                }        
                break;
                
        case BCSACTION_FIRE:
            switch(clients[id].public_info.state){
                case BCSCLST_CONNECTED:
                    clients[id].public_info.state = BCSCLST_PLAYING;
                    serv_msg.type = htobe32(BCSREPLT_ACK);
                    break;

                case BCSCLST_PLAYING: //UNDEFINED
                    break;

                default: //nothing -> error
                    serv_msg.type = htobe32(BCSREPLT_NACK);
                }
                break;

        //case BCSACTION_STRAFE: //UNDEFINED
        //    serv_msg.type = htobe32(BCSREPLT_NACK);
        //    break;
        
        case BCSACTION_ROTATE: //UNDEFINED
            serv_msg.type = htobe32(BCSREPLT_NACK);
            break;

        case BCSACTION_REQSTATS: //UNDEFINED
            serv_msg.type = htobe32(BCSREPLT_NACK);
            break;

        default:
            serv_msg.type = htobe32(BCSREPLT_NACK);
    }
    __syscall(sendto(u_fd, &serv_msg, sizeof(BCSMSGREPLY), 0, (struct sockaddr *) &addr_client, addr_size));
	*/
}
