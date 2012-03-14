#include "uinput.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

static const char *uinput_filename[] = {"/dev/uinput", "/dev/input/uinput", "/dev/misc/uinput"};

using std::cerr;
using std::endl;

UInput::UInput(const char *dev_name, std::vector<__u16> keys) : fd(-1) {
	openAll();
	setup(dev_name, keys);
	create();
}

UInput::~UInput() {
	destroy();
}

int UInput::open(const char *uinput_path) {
	this->fd = ::open(uinput_path, O_WRONLY);
	if(this->fd < 0) {
		return errno;
	}
	return 0;
}

/**
 * Try each uinput until one works
 */
void UInput::openAll() {
	int ret = ENOENT;
	for (int i = 0; i < 3; i++) {
		ret = open(uinput_filename[i]);

		// If all things worked, then bail
		if (ret == 0) {
			cerr << "\tOpened " << uinput_filename[i] << endl;
			break;
		}

		// If the device isn't found, try the next one
		if (ret == ENOENT)
			continue;

		if (ret == EACCES) {
			cerr << "Permission denied. Check you have permission to uinput." << endl;
		} else {
			cerr << errno << " " << strerror(errno) << endl;
		}

		throw std::runtime_error("Failed to open uinput");
	}

	if (ret != 0) {
		cerr << "uinput was not found. Is the uinput module loaded?" << endl;
		throw std::runtime_error("Failed to open uinput");
	}
}

void UInput::setup(const char *dev_name, std::vector<__u16> keys) {

	int ret;
	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));

	strncpy(uidev.name, dev_name, UINPUT_MAX_NAME_SIZE);
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 1;
	uidev.id.product = 1;
	uidev.id.version = 1;

	ret = write(this->fd, &uidev, sizeof(uidev));
	if (ret != sizeof(uidev)) {
		throw std::runtime_error("Failed to setup uinput");
		//cerr << "Failed to setup uinput: " << errno << " " << strerror(errno) << endl;
	}

	// We only want to send keypresses
	ret  = ioctl(this->fd, UI_SET_EVBIT, EV_KEY);

	// Add all the keys we might use
	for (std::vector<__u16>::const_iterator i = keys.begin(); i != keys.end(); i++) {
		if (*i != KEY_RESERVED)
			ret |= ioctl(this->fd, UI_SET_KEYBIT, *i);
	}

	if (ret) {
    	throw std::runtime_error("Failed to setup uinput");
		//cerr << "Failed to setup uinput" " << errno << " " << strerror(errno) << endl;
	}
}

void UInput::create() {
	int ret = ioctl(this->fd, UI_DEV_CREATE);
	if (ret) {
		throw std::runtime_error("Failed to create uinput");
	}

	cerr << "Created uinput device" << endl;

	// This sleep is here, because (for some reason) you need to wait before
	// sending you first uinput event.
	sleep(1);
}

void UInput::send_event(__u16 type, __u16 code, __s32 value) const {
	struct input_event ev;
	memset(&ev, 0, sizeof(ev));

	ev.type  = type;
	ev.code  = code;
	ev.value = value;

	int ret = write(this->fd, &ev, sizeof(ev));
	if (ret != sizeof(ev)) {
		throw std::runtime_error("Failed to send_event");
	}
}

void UInput::sync() const {
	send_event(EV_SYN, SYN_REPORT, 0);
}


void UInput::destroy() {
	ioctl(this->fd, UI_DEV_DESTROY);
	close(this->fd);

	this->fd = -1;
}
