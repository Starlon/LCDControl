
#ifndef __DRV_PICO_GRAPHIC_H__
#define __DRV_PICO_GRAPHIC_H__

#include <QObject>
#include <QThread>
#include <libusb-1.0/libusb.h>

#include "LCDGraphic.h"
#include "LCDCore.h"
#include "LCDControl.h"
#include "debug.h"

#define VENDOR_ID 0x04d8
#define PRODUCT_ID 0xc002
#define INTERFACE_ID 0
#define OUT_REPORT_LED_STATE 0x81
#define OUT_REPORT_LCD_BACKLIGHT 0x91
#define OUT_REPORT_LCD_CONTRAST 0x92
#define OUT_REPORT_CMD 0x94
#define OUT_REPORT_DATA 0x95
#define OUT_REPORT_CMD_DATA 0x96
#define _USBLCD_MAX_DATA_LEN 24
#define IN_REPORT_KEY_STATE 0x11
#define SCREEN_H 64
#define SCREEN_W 256
#define LIBUSB_ENDPOINT_IN 0x80
#define LIBUSB_ENDPOINT_OUT 0x00

namespace LCD {

class PGInterface {
    public:
    virtual ~PGInterface() {}
    virtual void DrvUpdateImg() = 0;
};

class PGWrapper : public QObject {
    Q_OBJECT
    PGInterface *wrappedObject_;
    public:
    PGWrapper(PGInterface *obj) { wrappedObject_ = obj; }
    public slots:
    void DrvUpdateImg() { wrappedObject_->DrvUpdateImg(); }
};

class PGSendCallback {
    protected:
    unsigned char *data_;
    int size_;
    public:
    PGSendCallback(unsigned char *data, int size) { data_ = data; size_ = size;}
    virtual ~PGSendCallback() { delete data_; }
    virtual void operator()() = 0;
};

class DrvPicoGraphic;

class PGCommandThread;
class PGPollThread;

class DrvPicoGraphic : public LCDCore, public LCDGraphic,
    public PGInterface {

    PGWrapper *wrapper_;
    QTimer *update_timer_;
    std::deque<PGSendCallback *> command_queue_;
    PGPollThread *poll_thread_;
    PGCommandThread *command_thread_;
    int refresh_rate_;
    bool *drvFB;
    bool inverted_;
    bool connected_;
    bool send_locked_;
    bool read_locked_;
    int transfers_out_;
    int sent_;
    int received_;
    int contrast_;
    int backlight_;
    int gpo_;
    struct libusb_device_handle *devh_;
    struct libusb_context *ctx_;
    struct libusb_transfer *send_transfer_;
    struct libusb_transfer *read_transfer_;

    void InitiateContrastBacklight();
    void DrvClear();
    void DrvRead(unsigned char *data, int size, void (*cb)(libusb_transfer*));
    void DrvSend(unsigned char *data, int size);
    void DrvUpdateImg();
    void DrvContrast(const int contrast);
    void DrvBacklight(const int backlight);
    void DrvUpdate();

    public:
    DrvPicoGraphic(std::string name, LCDControl *v,
        Json::Value *config, int layers);
    ~DrvPicoGraphic();    
    void DrvGPI();
    void DrvGPO(const int num, const int val);
    void CommandWorker();
    void SetupDevice();
    void TakeDown();
    void CFGSetup();
    void Connect();
    void Disconnect();
    void DrvPoll();
    void SetReadLocked(const bool val) { read_locked_ = val; }
    void SetSendLocked(const bool val) { send_locked_ = val; }
    void SignalFinished() { transfers_out_--; received_++; }
    void DrvKeypadEvent(const int key);
    void DrvBlit(const int row, const int col, const int height, const int width);


};

class PGCommandThread : public QThread {
    Q_OBJECT
    DrvPicoGraphic *visitor_;

    protected:
    void run() { visitor_->CommandWorker(); }

    public:
    PGCommandThread(DrvPicoGraphic *v) { visitor_ = v; }
};


class PGPollThread : public QThread {
    Q_OBJECT
    DrvPicoGraphic *visitor_;

    protected:
    void run() { visitor_->DrvPoll(); }

    public:
    PGPollThread(DrvPicoGraphic *v){ visitor_ = v; }
};


}; // End namespace

#endif
