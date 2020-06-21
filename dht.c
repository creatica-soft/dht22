// compile with
// gcc -O -o dht dht.c
// run with
// sudo chrt 99 ./dht <GPIO_PIN> <SAMPLING_INTERVAL>

#define DEVICE "/dev/gpiomem"
#define SAMPLING_RATE_SEC 1 //in seconds
#define PULSE_SAMPLING_DELAY_US 1 //in microseconds
#define MEDIAN 50 //us high voltage pulse length: short pulse for "0" is 26-28us, long pulse for "1" is 70us; low voltage pulse is 50us - used as a divider
#define ACK_RESPONSE_TIME 80 //us
#define DEVIATION 30 //us
#define REQUEST_US 18000 //us - request delay 18ms
#define DATA_TIMEOUT_US 150 //ns - data timeout
#define GPIOMEM_SIZE 0xB4
#define GPFSEL0 0 //GPIO function select 0
#define GPFSEL1 0x4 //GPIO function select 1
#define GPFSEL2 0x8 //GPIO function select 2
#define GPFSEL3 0xC //GPIO function select 3
#define GPFSEL4 0x10 //GPIO function select 4
#define GPFSEL5 0x14 //GPIO function select 5
//function select mask
//GPFSEL0 applies to GPIO0-GPIO9
//GPSEL1 applies to GPIO10-GPIO19
//GPSEL2 applies to GPIO20-GPIO29
//GPSEL3 applies to GPIO30-GPIO39
//GPSEL4 applies to GPIO40-GPIO49
//GPSEL5 applies to GPIO50-GPIO53
#define FINPUT 0
#define FOUTPUT 1
#define FUNC0 4
#define FUNC1 5
#define FUNC2 6
#define FUNC3 7
#define FUNC4 3
#define FUNC5 2
#define GPSET0 0x1C
#define GPSET1 0x20
#define GPCLR0 0x28
#define GPCLR1 0x2C
#define GPLEV0 0x34
#define GPLEV1 0x38


#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

long data[41];

void print_error(unsigned int i) {
	printf("error: missing data bit %u\n", i);
}

void process_data() {
	char deg[3] = { 0xc2, 0xb0, 0 }; //unicode degree symbol
	int16_t th = 0, tl = 0, hh = 0, hl = 0, cs = 0;
	
	//printf("data[%u] = %ld\n", 0, data[0]);
	if (data[0] < ACK_RESPONSE_TIME - DEVIATION || data[0] > ACK_RESPONSE_TIME + DEVIATION) {
		printf("Wrong ack response time %ldus, should be %u +/- %uus\n", data[0], ACK_RESPONSE_TIME, DEVIATION);
	}

	for (unsigned int i = 1; i < 9; i++) {
		//printf("data[%u] = %ld\n", i, data[i]);
		hh <<= 1;
		if (data[i] > MEDIAN) hh |= 1;
		else if (data[i] == 0) print_error(i);
	}
	for (unsigned int i = 9; i < 17; i++) {
		//printf("data[%u] = %ld\n", i, data[i]);
		hl <<= 1;
		if (data[i] > MEDIAN) hl |= 1;
		else if (data[i] == 0) print_error(i);
	}

	for (unsigned int i = 17; i < 25; i++) {
		//printf("data[%u] = %ld\n", i, data[i]);
		th <<= 1;
		if (data[i] > MEDIAN) th |= 1;
		else if (data[i] == 0) print_error(i);
	}

	for (unsigned int i = 25; i < 33; i++) {
		//printf("data[%u] = %ld\n", i, data[i]);
		tl <<= 1;
		if (data[i] > MEDIAN) tl |= 1;
		else if (data[i] == 0) print_error(i);
	}

	for (unsigned int i = 33; i < 41; i++) {
		//printf("data[%u] = %ld\n", i, data[i]);
		cs <<= 1;
		if (data[i] > MEDIAN) cs |= 1;
		else if (data[i] == 0) print_error(i);
	}
	if (((hh + hl + th + tl) & 0xff) != cs) {
		printf("Checksum error: hh = %u, hl = %u, th = %u, tl = %u, cs = %u\n", hh, hl, th, tl, cs);
	}
	else {
		printf("T = %.1f%sC, H = %.1f%%\n", ((th << 8) | tl) * 0.1, deg, ((hh << 8) | hl) * 0.1);
	}
}

int main(int argc, char ** argv) {
	struct timeval t1, t2;
	int fd, fd1;
	ssize_t res;
	unsigned int sampling_rate_sec = SAMPLING_RATE_SEC;

	if (argc > 3 || argc < 2) {
		printf("Usage: dht <gpio_pin> [sampling_rate_sec]\n");
		return -1;
	}

	if (!isdigit(argv[1][0])) {
		perror("GPIO pin is not a number");
		return -1;
	}
	unsigned int pin = atoi(argv[1]);
	if (argc == 3) sampling_rate_sec = atoi(argv[2]);
	printf("sampling rate is %us\n", sampling_rate_sec);
	printf("pulse timeout is %uus\n", DATA_TIMEOUT_US);
	printf("pulse sampling delay is %uus\n", PULSE_SAMPLING_DELAY_US);

	//long pagesize = sysconf(_SC_PAGE_SIZE);
	//printf("pagesize %ld\n", pagesize);

	fd = open(DEVICE, O_RDWR | O_SYNC);
	//fd = open(DEVICE, O_RDWR);
	if (fd < 0) {
		printf("error opening device %s\n", DEVICE); return -1;
	}

	char * gpio_reg = mmap(NULL, GPIOMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!gpio_reg) {
		perror("mmap failed");
		goto exit_on_error;
	}
	//printf("gpio_reg %lu\n", (uint64_t)gpio_reg);

	char *fsel_reg, *set_reg, *clr_reg, *get_reg;
	unsigned int fsel_mask, fsel_val = 0, shift, val, v;
/*
	for (int i = 0; i < 6; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPFSEL%d %u\n", i, val);
	}
	for (int i = 13; i < 15; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPLEV %u\n", val);
	}
	for (int i = 16; i < 18; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPEDS %u\n", val);
	}
	for (int i = 19; i < 21; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPREN %u\n", val);
	}
	for (int i = 22; i < 24; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPFEN %u\n", val);
	}
	for (int i = 25; i < 27; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPHEN %u\n", val);
	}
	for (int i = 28; i < 30; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPLEN %u\n", val);
	}
	for (int i = 31; i < 33; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPAREN %u\n", val);
	}
	for (int i = 34; i < 36; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPAFEN %u\n", val);
	}
	memcpy(&val, gpio_reg + 4 * 37, sizeof(val));
	printf("GPPUD %u\n", val);

	for (int i = 38; i < 40; i++) {
		memcpy(&val, gpio_reg + 4 * i, sizeof(val));
		printf("GPPUDCLK %u\n", val);
	}
	memcpy(&val, gpio_reg + 4 * 41, sizeof(val));
	printf("val %u\n", val);
*/
	switch (pin / 10) {
	case 0: fsel_reg = gpio_reg + GPFSEL0; break;
	case 1: fsel_reg = gpio_reg + GPFSEL1;	break;
	case 2: fsel_reg = gpio_reg + GPFSEL2; break;
	case 3: fsel_reg = gpio_reg + GPFSEL3; break;
	case 4: fsel_reg = gpio_reg + GPFSEL4; break;
	case 5: fsel_reg = gpio_reg + GPFSEL5; break;
	default: 
		perror("GPIO pin number range is from 0 to 53");
		goto exit_on_error;
	}
	//printf("fsel_reg %lu\n", (uint64_t)fsel_reg);

	shift = 3 * (pin % 10);
	//printf("shift %u\n", shift);
	fsel_mask = ~(7 << shift);
	//printf("fsel_mask %u\n", fsel_mask);

	memcpy(&fsel_val, fsel_reg, sizeof(fsel_val));
	//printf("fsel_val %u\n", fsel_val);
	fsel_val &= fsel_mask;
	//printf("fsel_val & fsel_mask %u\n", fsel_val);
	fsel_val |= (FOUTPUT << shift);
	//printf("fsel_val Ored with shifted FOUTPUT %u\n", fsel_val);
	if (pin < 32) {
		set_reg = gpio_reg + GPSET0;
		clr_reg = gpio_reg + GPCLR0;
		get_reg = gpio_reg + GPLEV0;
		val = 1 << pin;
	}
	else {
		set_reg = gpio_reg + GPSET1;
		clr_reg = gpio_reg + GPCLR1;
		get_reg = gpio_reg + GPLEV1;
		val = 1 << (pin - 32);
	}
	//printf("set_reg %lu\n", (uint64_t)set_reg);
	//printf("clr_reg %lu\n", (uint64_t)clr_reg);
	//printf("get_reg %lu\n", (uint64_t)get_reg);
	//printf("val %u\n", val);

	while (1) {
		sleep(sampling_rate_sec);

		//setting GPIO pin as an output
		memcpy(fsel_reg, &fsel_val, sizeof(fsel_val));

		//setting GPIO pin high
		memcpy(set_reg, &val, sizeof(val));

		usleep(REQUEST_US);

		//setting GPIO pin low
		memcpy(clr_reg, &val, sizeof(val));
		usleep(REQUEST_US);

		//setting GPIO pin high
		memcpy(set_reg, &val, sizeof(val));
		usleep(30);

		//setting GPIO pin as an input
		memcpy(&v, fsel_reg, sizeof(v));
		v &= fsel_mask;
		memcpy(fsel_reg, &v, sizeof(v));

		long timeout;
		for (unsigned int i = 0; i < 41; i++) {
			timeout = 0;
			memcpy(&v, get_reg, sizeof(v));
			v &= val;
			v >>= pin;
			while (v == 0 && timeout++ < DATA_TIMEOUT_US) {
				usleep(PULSE_SAMPLING_DELAY_US);
				memcpy(&v, get_reg, sizeof(v));
				v &= val;
				v >>= pin;
			}
			timeout = 0;
			gettimeofday(&t1, NULL);
			while (v == 1 && timeout++ < DATA_TIMEOUT_US) {
				usleep(PULSE_SAMPLING_DELAY_US);
				memcpy(&v, get_reg, sizeof(v));
				v &= val;
				v >>= pin;
			}
			gettimeofday(&t2, NULL);
			data[i] = t2.tv_usec - t1.tv_usec;
			if (data[i] < 0) data[i] += 1000000L;
		}
		process_data();
	}

exit_on_error:
	if (gpio_reg) munmap(gpio_reg, GPIOMEM_SIZE);
	close(fd);
	return 0;
}
