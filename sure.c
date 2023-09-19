/* MODIFY THIS FILE
 * together with sure.c, this implements the SURE protocol
 * for reliable data transfer.
 */
/* The comments in the functions are just hints on what to do,
 * but you are not forced to implement the functions as asked
 * (as long as you respect the interface described in sure.h
 * and that your protocol works)
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include "sure.h"

void udt_disable_timeout(udt_socket_t *s)
{
    int ret;
    struct timeval timeout;

    // set timeout to zero
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // disable timeout
    ret = setsockopt(s->s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret == -1)
    {
        perror("UDT - setsockopt");
    }
}
void sender_side(sure_socket_t *p)
{
    while (true)
    {

        int ret = udt_recv(&p->udt, p->sure_buffer, HEADER);
        printf("IN sender_side : ret=%d  udtside:%d\n", ret, p->udt.side);
        if (ret == -2)
        {
            pthread_mutex_unlock(&p->mutex_read);
            pthread_mutex_unlock(&p->mutex_timeout_send);
            printf("IN sender_side : ça a timeout \n");
        }
        else if (ret == -1)
        {
            pthread_mutex_unlock(&p->mutex_read);
            printf("IN sender_side : perte du socket\n ");
        }
        else
        {

            char metat = (p->sure_buffer->etat);
            short ack = p->sure_buffer->ACK;
            short datasize = p->sure_buffer->datasize;
            printf("IN sender_side : packet etat = %c  ack=%d data size=%d\n", metat, ack, datasize);

            if (metat == '0' && ack == p->ack_num + 1)
            {
                printf("IN sender_side : move connect to 1\n");
                p->connect = 1;
                pthread_mutex_unlock(&p->mutex_read);
            }
            else if (metat == '1')
            {
                printf("IN sender_side : reception de ack \n");
                // Reception des ack et avancement windows
                if (p->cur >= p->pro)
                {
                    // on a ratraper entierement pour laisser l'arret de sure_write a lui meme on repart vers lui
                    // pas opti
                    pthread_mutex_unlock(&p->mutex_timeout_send);
                }
                else if (ack < (short)p->cur)
                {
                    // ignore
                    continue;
                }
                else if (ack >= (short)p->cur)
                {
                    int intermediaire = p->pro;
                    if ((ack + SURE_WINDOW) < p->cptpacket)
                    {
                        p->pro = ack + SURE_WINDOW + 1;
                    }
                    else
                    {
                        p->pro = p->cptpacket;
                    }
                    p->cur = ack + 1;
                    printf("IN sender_side : cur%d intermed%d et pro:%d\n", p->cur, intermediaire, p->pro);
                    // on renvoie les packet cur+WINDOW et cur+1 et pro+1
                    for (int i = intermediaire; i < p->pro; i++)
                    {
                        udt_send(&p->udt, &p->bigbuffer[i], HEADER + p->bigbuffer[i].datasize);
                    }
                }
                // util?
                pthread_mutex_unlock(&p->mutex_read);
            }
            else if (metat == '2')
            { // tous fermer et attendre que l'autre ack de tout fermer
                printf("IN sender_side :fin demander par receiver \n");
                pthread_mutex_unlock(&p->mutex_read);
            }
            else
            { // probleme d'etat
                printf("IN sender_side :etat diff de 0 1 2 \n");
                pthread_mutex_unlock(&p->mutex_read);
            }
        }
    }
}

void receiver_side(sure_socket_t *p)
{

    while (!(p->timeout))
    {
        pthread_mutex_lock(&p->mutex_write);
        pthread_mutex_unlock(&p->mutex_write);

        // sleep(1);
        int ret = udt_recv(&p->udt, &p->sure_buffer, SURE_PACKET_SIZE);
        if (ret == -1)
        {
            pthread_mutex_unlock(&p->mutex_read);
            printf("IN receiver_side : ret -1\n");
        }
        if (ret == -2)
        {
            pthread_mutex_unlock(&p->mutex_read);
            printf("IN receiver_side : ret -2\n");
        }
        else
        {

            printf("IN receiver_side : msg recu avec ret:%d\n", ret);
            char metat = p->sure_buffer->etat;
            short ack = p->sure_buffer->ACK;
            short datasize = p->sure_buffer->datasize;
            printf("IN receiver_side  : packet etat = %c  ack=%d data size=%d acknum=%d\n", metat, ack, datasize, p->ack_num);
            if (metat == '0')
            {
                p->sure_buffer->ACK = (p->sure_buffer->ACK) + 1;

                printf("IN receiver_side :connect to 1 send ack initalisation\n");
                p->connect = 1;
                udt_send(&p->udt, p->sure_buffer, HEADER);
                pthread_mutex_unlock(&p->mutex_read);
            }
            else if (metat == '1')
            { // renvoi ack
                pthread_mutex_lock(&p->mutex_read);
                sure_packet_t packetack;
                packetack.ACK = p->ack_num;
                packetack.etat = '1';
                packetack.datasize = 0;

                printf("IN receiver_side : renvoie ack\n");
                // garder msg si bon
                // un problem de parema sur acknum de 0
                // solve just pour lui
                if (p->sure_buffer->ACK == p->ack_num)
                {
                    // recup fichier
                    printf("IN receiver_side : recpet buffer copie to big buff\n ");
                    memcpy(&p->bigbuffer[p->ack_num], &p->sure_buffer, ret);
                    // ack pour packet suivant
                    udt_send(&p->udt, &packetack, HEADER);
                    p->ack_num++;
                    // dire recu packet a sure_read
                    pthread_cond_broadcast(&p->cond_init);
                }
                else
                {
                    if (p->ack_num == 0)
                    {
                        packetack.ACK = p->ack_num - 1;
                        udt_send(&p->udt, &packetack, HEADER);
                    }
                    else
                    {
                        packetack.ACK = p->ack_num - 1;
                        udt_send(&p->udt, &packetack, HEADER);
                    }
                }

                pthread_mutex_unlock(&p->mutex_read);
            }
            else if (metat == '2')
            {
                printf("IN receiver_side :fin demander par sender\n");
                // ici envoyer recevoir le close et timeout la boucle while
                pthread_mutex_unlock(&p->mutex_read);
            }
            else
            {
                printf("IN receiver_side :etat diff de 0 1 2 \n");
                pthread_mutex_unlock(&p->mutex_read);
            }
        }
    }
}

int sure_read(sure_socket_t *s, char *msg, int msg_size)
{
    // wait if there isn't anything in the buffer (we'll be signaled by the other thread)
    // if we are not connected, return 0
    // take as many packets as there are in the buffer and append
    // them into the message that will be returned to the application (respecting the
    // msg_size limitation)
    // return the number of bytes written to msg
    // printf("AVANT LE MUTEXLOCK \n");
    // printf("APRES LE MUTEXLOCK avec cpt = %d et msg_size=%d et cpt/DATASIZE = %d\n", cpt, msg_size, cpt % DATASIZE);

    s->ack_num = 0;
    s->cpt = 0; // nbr de charactère écrit

    while (s->cpt < msg_size && !(s->timeout) && s->cpt % DATASIZE == 0 && s->ack_num < 64)
    {
        printf("IN sure_read : en attente de remplissage ! \n");
        pthread_cond_wait(&s->cond_init, &s->mutex_read);
        printf("IN sure_read : message reçu ! datasize=%d\n", s->bigbuffer[s->ack_num - 1].datasize);
        // memcpy(msg + s->cpt, s->bigbuffer[s->ack_num - 1].data, s->bigbuffer[s->ack_num - 1].datasize);
        printf("IN sure_read : cpt = %d et dat size=%hu ack num copier:%d\n", s->cpt, s->bigbuffer[s->ack_num - 1].datasize, (s->ack_num - 1));
        s->cpt = s->cpt + (int)s->bigbuffer[s->ack_num - 1].datasize;
    }
    printf("IN sure_read : fin de la reception dans bigbuff");

    pthread_mutex_lock(&s->mutex_write);
    pthread_mutex_unlock(&s->mutex_read);

    for (int i = 0; i < (s->ack_num); i++)
    {
        memcpy(msg + ((SURE_PACKET_SIZE - HEADER) * i), s->bigbuffer[i].data, s->bigbuffer[i].datasize);
    }
    printf("IN sure_read : fin copie bigbuff to msg\n");
    pthread_mutex_unlock(&s->mutex_write);
    // s->cur = 0;
    // s->pro = SURE_WINDOW;
    sleep(6);
    return s->cpt;
}

void print_buffer(char data[], int size)
{
    int i;
    for (i = 0; i < size; i++)
    {
        printf("%c", data[i]);
    }
    printf("\n");
}

int sure_write(sure_socket_t *s, char *msg, int msg_size)
{
    sleep(1);
    // break the application message into multiple SURE packets
    // add them to the buffer (wait if the buffer is full)
    // must do a memory copy because the application buffer may be reused right away
    // send the packets that fall within the window
    // reset var utiliser
    s->cur = 0;
    s->pro = SURE_WINDOW;

    int ack_indice = 0;

    short taille_msg_glob = (SURE_PACKET_SIZE - HEADER);
    int avancement = 0;
    s->cptpacket = 0;
    if (taille_msg_glob * 64 < msg_size)
    {
        printf("IN sure_write : msg Taille trop grande !\n");
        // Plus tard : appeler sure_write en découpant en plusieurs petits messages
    }
    else
    {
        while (msg_size - avancement >= (int)taille_msg_glob) // gerer si indice depasse 64 ou pas ?
        {

            s->bigbuffer[ack_indice].ACK = (short)ack_indice;
            s->bigbuffer[ack_indice].etat = '1';
            s->bigbuffer[ack_indice].datasize = (short)taille_msg_glob;

            memcpy(s->bigbuffer[ack_indice].data, msg + avancement, taille_msg_glob);
            printf("IN sure_write :packet construit ack:%d,etat:%c,datasize:%d \n", s->bigbuffer[ack_indice].ACK, s->bigbuffer[ack_indice].etat, s->bigbuffer[ack_indice].datasize);
            s->cptpacket++;
            avancement = avancement + taille_msg_glob;
            ack_indice++;
        }

        if (msg_size - avancement > 0)
        {
            printf("IN sure_write :demi packet\n");
            s->bigbuffer[ack_indice].ACK = (short)ack_indice;
            s->bigbuffer[ack_indice].etat = '1';
            s->bigbuffer[ack_indice].datasize = (short)(msg_size % taille_msg_glob); // le reste
            printf("datasize%d\n",s->bigbuffer[ack_indice].datasize);
            memcpy(s->bigbuffer[ack_indice].data, msg + avancement, msg_size-avancement);
            printf("IN sure_write :packet construit ack:%d,etat:%c,datasize:%d \n", s->bigbuffer[ack_indice].ACK, s->bigbuffer[ack_indice].etat, s->bigbuffer[ack_indice].datasize);

            s->cptpacket++;
            avancement = avancement + (msg_size % taille_msg_glob);
            // ack_indice++;
        }

        printf("IN sure_write : fin de copi msg  to bigbuffer ,cptpacket = %d, ackmax=%d\n", s->cptpacket, ack_indice);
        s->ack_num = 0;

        if (s->pro > s->cptpacket)
        {
            s->pro = s->cptpacket;
        }
        while (s->cur < s->pro && s->cur < SURE_BUFFER && s->cur < s->cptpacket)

        { // Ne pas oublier de faire en sorte d'arrêter quand on atteint des buffers full vide

            printf("IN sure_write :en voie window, cur = %d,pro:%d\n", s->cur, s->pro);
            pthread_mutex_lock(&s->mutex_timeout_send);
            for (int i = s->cur; i < s->pro; i++)
            {
                udt_send(&s->udt, &s->bigbuffer[i], HEADER + s->bigbuffer[i].datasize);
            }
        }
    }

    printf("IN sure_write : avcenment=:%d", avancement);
    sleep(6);
    return avancement;
}

int sure_init(char *receiver, int port, int side, sure_socket_t *p)
{
    // fill the sure_socket_t
    // call udt_init
    // start thread (the receiver will need a thread that receives packets and add them to a buffer, the sender will need a thread that receives acks and removes packets from the buffer, or that retransmits if needed)
    // start connection (and wait until it is established)

    p->connect = 0;
    p->timeout = false;
    pthread_cond_init(&p->cond_init, NULL);
    pthread_cond_init(&p->cond_connec, NULL);
    printf("IN sure_init : START SUR INIT\n");
    // Start thread for receiver or sender
    if (side == SURE_RECEIVER)
    {
        pthread_mutex_init(&p->mutex_read, NULL);
        pthread_mutex_init(&p->mutex_write, NULL);
    }
    else if (side == SURE_SENDER)
    {
        pthread_mutex_init(&p->mutex_read, NULL);
        pthread_mutex_init(&p->mutex_timeout_send, NULL);
    }
    // start socket udt
    udt_init(receiver, port, side, &p->udt);

    // Start threads
    if (side == SURE_RECEIVER)
    {
        pthread_create(&p->thread, NULL, receiver_side, p);
    }
    else if (side == SURE_SENDER)
    {
        udt_set_timeout(&p->udt, SURE_TIMEOUT);
        pthread_create(&p->thread, NULL, sender_side, p);
    }

    // Start Connection
    if (side == SURE_SENDER)
    {
        int i = 0;
        // while (p->connect == 0 && i<SENDER_TMAX) //Avec time out si pas de recepteur 10 fois
        while (p->connect == 0)
        {
            pthread_mutex_lock(&p->mutex_read);
            i++;
            p->ack_num = (rand() % 254);
            printf("IN sure_init : connectack=%i\n", p->ack_num);
            sure_packet_t packet;
            packet.ACK = p->ack_num;
            packet.etat = '0';
            packet.datasize = 0;
            udt_send(&p->udt, &packet, HEADER);

            printf("IN sure_init : unlock avec connect = %d\n", p->connect);
        }
        printf("IN sure_init : COTER SENDER SORTIE BOUCLE\n");
        // if (i >= SENDER_TMAX)
        // {
        //     printf("IN sure_init :time max sans conenction\n");
        //     return SURE_FAILURE;
        // }
    }
    else if (side == SURE_RECEIVER)
    {
        printf("IN sure_init : recieve side connect\n");
        // peut etre ajouter time out max pour pas attendrre a l'infini
        // avec return SURE FAILURE

        while (p->connect == 0)
        {
            printf("IN sure_init : dans boucle receiver\n");
            pthread_mutex_lock(&p->mutex_read);
            pthread_cond_broadcast(&p->cond_connec);
        }
        if (p->connect == 1)
        {
            printf("IN sure_init :receiver add time out\n");
            udt_set_timeout(&p->udt, SURE_TIMEOUT);
        }
        printf("IN sure_init : COTER RECEIVER SORTI BOUCLE\n");
    }
    return SURE_SUCCESS;
}

void sure_close(sure_socket_t *s)
{
    void *ret;
    if (s->udt.side == SURE_RECEIVER)
    {
        printf("IN sure_close :fin receiver");
        //renvoyer le ack et etre sur de la reception 
        //fermer la boucle while du thread avec le time out
        //pthread join
        udt_close(&s->udt);
        
    }
    else
    {
        //boucle while et mutex attendre que le ack de fin de sur receive revienne
        // udt_close(&s->udt);
        // pthread_exit(&s->thread);
        printf("IN sure_close :fin sender");
    }

    // end the connection (and wait until it is done)
    // call udt_close
    // call pthread_join for the thread
}
