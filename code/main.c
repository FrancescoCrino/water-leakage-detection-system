#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "periph/gpio.h"
#include "xtimer.h"
#include "ztimer.h"

#include "shell.h"
#include "msg.h"
#include "net/emcute.h"
#include "net/ipv6/addr.h"

#ifndef EMCUTE_ID
#define EMCUTE_ID           ("gertrud")
#endif
#define EMCUTE_PRIO         (THREAD_PRIORITY_MAIN - 1)

#define NUMOFSUBS           (16U)
#define TOPIC_MAXLEN        (64U)

static char stack[THREAD_STACKSIZE_DEFAULT];
static msg_t queue[8];

static emcute_sub_t subscriptions[NUMOFSUBS];

#define ADC_IN_USE_1  ADC_LINE(0)
#define ADC_IN_USE_2  ADC_LINE(1)
#define ADC_RES     ADC_RES_6BIT

gpio_t pin_buzzer;

// --- connectivity functions ---

static void *emcute_thread(void *arg){
    (void)arg;
    emcute_run(CONFIG_EMCUTE_DEFAULT_PORT, EMCUTE_ID);
    return NULL;    /* should never be reached */
}

static unsigned get_qos(const char *str){
    int qos = atoi(str);
    switch (qos) {
        case 1:     return EMCUTE_QOS_1;
        case 2:     return EMCUTE_QOS_2;
        default:    return EMCUTE_QOS_0;
    }
}

static int cmd_con(int argc, char **argv){
    sock_udp_ep_t gw = { .family = AF_INET6, .port = CONFIG_EMCUTE_DEFAULT_PORT };
    char *topic = NULL;
    char *message = NULL;
    size_t len = 0;

    if (argc < 2) {
        printf("usage: %s <ipv6 addr> [port] [<will topic> <will message>]\n",
                argv[0]);
        return 1;
    }

    /* parse address */
    if (ipv6_addr_from_str((ipv6_addr_t *)&gw.addr.ipv6, argv[1]) == NULL) {
        printf("error parsing IPv6 address\n");
        return 1;
    }

    if (argc >= 3) {
        gw.port = atoi(argv[2]);
    }
    if (argc >= 5) {
        topic = argv[3];
        message = argv[4];
        len = strlen(message);
    }

    if (emcute_con(&gw, true, topic, message, len, 0) != EMCUTE_OK) {
        printf("error: unable to connect to [%s]:%i\n", argv[1], (int)gw.port);
        return 1;
    }
    printf("Successfully connected to gateway at [%s]:%i\n",
           argv[1], (int)gw.port);

    return 0;
}

static int cmd_pub(int argc, char **argv){
    emcute_topic_t t;
    unsigned flags = EMCUTE_QOS_0;

    if (argc < 3) {
        printf("usage: %s <topic name> <data> [QoS level]\n", argv[0]);
        return 1;
    }

    /* parse QoS level */
    if (argc >= 4) {
        flags |= get_qos(argv[3]);
    }

    printf("pub with topic: %s and name %s and flags 0x%02x\n", argv[1], argv[2], (int)flags);

    /* step 1: get topic id */
    t.name = argv[1];
    if (emcute_reg(&t) != EMCUTE_OK) {
        puts("error: unable to obtain topic ID");
        return 1;
    }

    /* step 2: publish data */
    if (emcute_pub(&t, argv[2], strlen(argv[2]), flags) != EMCUTE_OK) {
        printf("error: unable to publish data to topic '%s [%i]'\n",
                t.name, (int)t.id);
        return 1;
    }

    printf("Published %i bytes to topic '%s [%i]'\n",
            (int)strlen(argv[2]), t.name, t.id);

    return 0;
}

static int publish(char *message){

    emcute_topic_t t;
    unsigned flags = EMCUTE_QOS_0;

    char *topic = "wl_sensors";

    //printf("pub with topic: %s, message: %s with flags: 0x%02x\n", topic, message, (int)flags);

    t.name = topic;
    if (emcute_reg(&t) != EMCUTE_OK) {
        puts("error: unable to obtain topic ID");
        return 1;
    }

    //publish data
    if (emcute_pub(&t, message, strlen(message), flags) != EMCUTE_OK) {
        printf("error: unable to publish data to topic '%s [%i]'\n",
                t.name, (int)t.id);
        return 1;
    }

    printf("\n Published %i bytes to topic '%s' \n",
            (int)strlen(message), t.name);

    return 0;
}

void sampling(void){

    int samplew = 0;
    int samplem = 0;
    int poss_leak = 0;
    char data[50];

    printf("\nStarting collecting Data: \n");

    while(1){
        samplew = adc_sample(ADC_IN_USE_1, ADC_RES);
        samplem = adc_sample(ADC_IN_USE_2, ADC_RES);
        if(samplew==63 && samplem==63){
            printf("\n WATER: %d - MOVEMENT: %d - LEAK: %d ", abs(samplew/63), abs(samplem/63), 0);
            sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
            publish((char *)&data);
            xtimer_sleep(20);
        }
        // If samplew or samplem are not high we sample each 10s
        else if(samplew!=63 || samplem!=63){
            // If samplew or samplem are not high, we check after 4 seconds
            xtimer_sleep(4);
            samplew = adc_sample(ADC_IN_USE_1, ADC_RES);
            samplem = adc_sample(ADC_IN_USE_2, ADC_RES);
            if(samplew==63 && samplem==63){
                //If the check is negative (water and mov == 63) we wait 16s (+4=20s)
                printf("\n WATER: %d - MOVEMENT: %d - LEAK: %d ", abs(samplew/63), abs(samplem/63), 0);
                sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
                poss_leak = 0;
                xtimer_sleep(6);
            }else if(samplew!=63 && samplem==63){
                printf("\n WATER: %d - MOVEMENT: %d - LEAK: 0 ", abs(samplew/63), abs(samplem/63));
                sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
                xtimer_sleep(16);
            }else if(samplew==63 && samplem!=63){
                // water flowing and no movement -> possible leak
                poss_leak = poss_leak +1;
                printf("\n --> POSSIBLE LEAK (%d) (WATER: %d - MOVEMENT: %d - LEAK: 0)", poss_leak, abs(samplew/63), abs(samplem/63));
                sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
                xtimer_sleep(6);
            }else{
                printf("\n WATER: %d - MOVEMENT: %d - LEAK: 0", abs(samplew/63), abs(samplem/63));
                sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
                xtimer_sleep(16);
            }
            if(poss_leak == 2){
                // After 2 possible leak -> leak detected
                poss_leak = 0;
                while(samplew==63){
                    printf("\n --> LEAK DETECTED!! PLEASE CLOSE THE TAP!!");
                    gpio_set(pin_buzzer);
                    sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":1}", abs(samplew/63), abs(samplem/63));
                    publish((char *)&data);
                    xtimer_sleep(5);
                    samplew = adc_sample(ADC_IN_USE_1, ADC_RES);
                    samplem = adc_sample(ADC_IN_USE_2, ADC_RES);
                    if(samplew!=63){
                        xtimer_sleep(2);
                        samplew = adc_sample(ADC_IN_USE_1, ADC_RES);
                        // If no more water flowing out of the tap -> No more leak
                        if(samplew!=63){
                            printf("\n\t\t TANK YOU FOR CLOSING THE TAP!!");
                            sprintf(data, "{\"water\":%d,\"movement\":%d,\"leakage\":0}", abs(samplew/63), abs(samplem/63));
                        }
                    }
                }
                gpio_clear(pin_buzzer);
            }
            else{
                publish((char *)&data);
            }
        }
    }
}

static int run(int argc, char **argv){

    if (argc > 1) {
        printf("No argument needed for command %s\n", argv[0]);
        return 1;
    }

    /* initialize gpio port out*/
    pin_buzzer = GPIO_PIN(PORT_F, 15); //D2

    if (gpio_init(pin_buzzer, GPIO_OUT)) {
        printf("Error to initialize GPIO_PIN(%d %d)\n", PORT_F, 15);
        return -1;
    }

    /* initialize the ADC line */
    if (adc_init(ADC_IN_USE_1) < 0) {
        printf("Initialization of ADC_LINE(%u) failed\n", ADC_IN_USE_1);
        return 1;

    } else {
        printf("Successfully initialized ADC_LINE(%u)\n", ADC_IN_USE_1);
    }

    if (adc_init(ADC_IN_USE_2) < 0) {
        printf("Initialization of ADC_LINE(%u) failed\n", ADC_IN_USE_2);
        return 1;

    } else {
        printf("Successfully initialized ADC_LINE(%u)\n", ADC_IN_USE_2);
    }

    sampling();

    return 0;

}

static const shell_command_t shell_commands[] = {
    { "con", "connect to MQTT broker", cmd_con },
    { "pub", "publish something", cmd_pub },
    { "run", "Start the water leakage detection system", run },
    { NULL, NULL, NULL }
};


int main(void){

    printf("\n --------------------- WATER LEAKAGE DETECTION SYSTEM  --------------------- \n");

    /* the main thread needs a msg queue to be able to run `ping`*/
    msg_init_queue(queue, ARRAY_SIZE(queue));

    /* initialize our subscription buffers */
    memset(subscriptions, 0, (NUMOFSUBS * sizeof(emcute_sub_t)));

    /* start the emcute thread */
    thread_create(stack, sizeof(stack), EMCUTE_PRIO, 0,
                  emcute_thread, NULL, "emcute");

    /* start shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;

}