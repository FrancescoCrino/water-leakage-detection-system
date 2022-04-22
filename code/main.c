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

#include <time.h>

#ifndef EMCUTE_ID
#define EMCUTE_ID           ("gertrud")
#endif
#define EMCUTE_PRIO         (THREAD_PRIORITY_MAIN - 1)

#define NUMOFSUBS           (16U)
#define TOPIC_MAXLEN        (64U)

static char stack[THREAD_STACKSIZE_DEFAULT];
static msg_t queue[8];

static emcute_sub_t subscriptions[NUMOFSUBS];
//static char topics[NUMOFSUBS][TOPIC_MAXLEN];

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
    return 0;
}

static int run(int argc, char **argv){

    if (argc > 1) {
        printf("No argument needed for command %s\n", argv[0]);
        return 1;
    }

    gpio_t pin_in_water = GPIO_PIN(PORT_F, 13); //D7
    gpio_t pin_in_motion = GPIO_PIN(PORT_F, 14); //D4
    gpio_t pin_led = GPIO_PIN(PORT_F, 15); //D2
    gpio_t pin_buzzer = GPIO_PIN(PORT_E, 9); //D2

    if (gpio_init(pin_in_water, GPIO_IN)) {
        printf("Error to initialize GPIO_PIN(%d %d)\n", PORT_F, 13);
        return -1;
    }
    if (gpio_init(pin_in_motion, GPIO_IN)) {
        printf("Error to initialize GPIO_PIN(%d %d)\n", PORT_F, 14);
        return -1;
    }
    if (gpio_init(pin_led, GPIO_OUT)) {
        printf("Error to initialize GPIO_PIN(%d %d)\n", PORT_F, 15);
        return -1;
    }
    if (gpio_init(pin_buzzer, GPIO_OUT)) {
        printf("Error to initialize GPIO_PIN(%d %d)\n", PORT_E, 9);
        return -1;
    }

    int in_value_water;
    int in_value_motion;
    printf("Reading water flux from pin D7 and motion sensor from pin D4 \n");

    time_t rawtime;
    struct tm * timeinfo;

    gpio_clear(pin_in_water);
    gpio_clear(pin_in_motion);

    while (1) {



        time(&rawtime);
        timeinfo = localtime(&rawtime);

        char data[50];

        int leak = 0;

        gpio_set(pin_led); // turn on led

        in_value_water = gpio_read(pin_in_water);
        in_value_motion = gpio_read(pin_in_motion);
        printf("\t input value: water: %d  -  motion: %d \n", in_value_water, in_value_motion);

        if(in_value_water > 0 && in_value_motion==0){
            int leak_count = 0;
            int water_off = 0;
            while(water_off < 3 || in_value_motion==0){
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                //gpio_clear(pin_buzzer);
                in_value_water = gpio_read(pin_in_water);
                in_value_motion = gpio_read(pin_in_motion);
                ztimer_sleep(ZTIMER_MSEC, 10 * 1000); // 5 seconds
                leak_count = leak_count + 1;
                if(leak_count > 3){
                    printf("\t\t  -----> LEAKAGE DETECTED: water: %d  -  motion: %d \n", in_value_water, in_value_motion);
                    // COmando per accendere buzzer
                    gpio_set(pin_buzzer); // turn on buzzer
                    printf("\t\t  -----> buzzer on \n");
                    leak = 1;
                    sprintf(data, "{\"time\":\"%d:%d:%d\", \"water\":%d,\"movement\":%d,\"leakage\":%d}", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, in_value_water, in_value_motion, leak);
                }else{
                    printf("\t\t Possible Leakage detected (%d): water: %d  -  motion: %d\n", leak_count, in_value_water, in_value_motion);
                    sprintf(data, "{\"time\":\"%d:%d:%d\", \"water\":%d,\"movement\":%d,\"leakage\":%d}", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, in_value_water, in_value_motion, leak);
                }
                if(in_value_water == 0){
                    water_off = water_off + 1;
                }
                publish((char *)&data);
                gpio_clear(pin_in_water);
                gpio_clear(pin_in_motion);
            }

        }
        else {
            gpio_clear(pin_buzzer);
            sprintf(data, "{\"time\":\"%d:%d:%d\", \"water\":%d,\"movement\":%d,\"leakage\":%d}", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, in_value_water, in_value_motion, leak);
            publish((char *)&data);
        }

        ztimer_sleep(ZTIMER_MSEC, 30 * 1000); // 5 seconds

        gpio_clear(pin_in_water);
        gpio_clear(pin_in_motion);

    }

    return 0;

}

static const shell_command_t shell_commands[] = {
    { "con", "connect to MQTT broker", cmd_con },
    { "pub", "publish something", cmd_pub },
    { "run", "Start the water leakage detection system", run },
    { NULL, NULL, NULL }
};


int main(void){

    printf("\n --------------------- WATER FLOW METER  --------------------- \n");

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