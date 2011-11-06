
#ifndef __DRV_NIFTY_H__
#define __DRV_NIFTY_H__

#include <QObject>
#include <QThread>
#include <string>
#include <libnifty/libnifty.h>
#include <libnifty/mutex.h>
#include <libnifty/client/remote.h>
#include <libnifty/client/input.h>


#include "LCDGraphic.h"
#include "LCDCore.h"
#include "LCDControl.h"
#include "RGBA.h"
#include "debug.h"

#define SCREEN_H 64
#define SCREEN_W 256

namespace LCD {

class NiftyInterface {
    public:
    virtual ~NiftyInterface() {}
    virtual void DrvUpdateImg() = 0;
};

class NiftyWrapper : public QObject {
    Q_OBJECT
    NiftyInterface *wrappedObject_;
    public:
    NiftyWrapper(NiftyInterface *obj) { wrappedObject_ = obj; }
    public slots:
    void DrvUpdateImg() { wrappedObject_->DrvUpdateImg(); }
};

class NiftySendCallback {
    protected:
    uint8_t *data_;
    int size_;
    public:
    NiftySendCallback(uint8_t *data, int size) { data_ = data; size_ = size;}
    virtual ~NiftySendCallback() { delete []data_; }
    virtual void operator()() = 0;
};

class DrvNifty;

class NiftyUpdateThread;

class DrvNifty : public LCDCore, public LCDGraphic,
    public NiftyInterface {

    nlNifty *nifty_;
    nlFrame *frame_;
    nlRemoteClient *client_;
    nlRemoteInput *input_;
    nlMutex *mutex_;

    int udp_port_;
    std::string udp_host_;

    NiftyWrapper *wrapper_;
    std::deque<NiftySendCallback *> command_queue_;
    NiftyUpdateThread *update_thread_;
    RGBA *drvFB;

    bool connected_;
    int update_;
    int cols_;
    int rows_;
    int bpp_;
    int depth_;

    void DrvClear();
    void DrvUpdateImg();
    void DrvUpdate();

    public:
    DrvNifty(std::string name, LCDControl *v,
        Json::Value *config, int layers);
    ~DrvNifty();    
    void SetupDevice();
    void TakeDown();
    void CFGSetup();
    void Connect();
    void Disconnect();
    void UpdateThread();
    void DrvBlit(const int row, const int col, const int height, const int width);

};

class NiftyUpdateThread : public QThread {
    Q_OBJECT
    DrvNifty *visitor_;

    protected:
    void run() { visitor_->UpdateThread(); }

    public:
    NiftyUpdateThread(DrvNifty *v) { visitor_ = v; }
};



}; // End namespace

#endif
