#include "../tausch.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "ctauschdouble.h"

CTauschDouble *tausch_new_double(size_t numBuffers, size_t *valuesPerPointPerBuffer, MPI_Comm comm) {
    Tausch<double> *t = new Tausch<double>(MPI_DOUBLE, numBuffers, (valuesPerPointPerBuffer==NULL ? nullptr : valuesPerPointPerBuffer), comm);
    return reinterpret_cast<CTauschDouble*>(t);
}

void tausch_delete_double(CTauschDouble *tC) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    delete t;
}

/*********************************************/
// tausch_setLocalHaloInfo*

// CPU with CPU
void tausch_setLocalHaloInfo1D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo1D_CwC(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo2D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo2D_CwC(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo3D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo3D_CwC(numHaloParts, haloSpecs);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_setLocalHaloInfo1D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo1D_CwG(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo2D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo2D_CwG(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo3D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo3D_CwG(numHaloParts, haloSpecs);
}

// GPU with CPU
void tausch_setLocalHaloInfo1D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo1D_GwC(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo2D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo2D_GwC(numHaloParts, haloSpecs);
}
void tausch_setLocalHaloInfo3D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setLocalHaloInfo3D_GwC(numHaloParts, haloSpecs);
}
#endif


/*********************************************/
// tausch_setRemoteHaloInfo*


// CPU with CPU
void tausch_setRemoteHaloInfo1D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo1D_CwC(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo2D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo2D_CwC(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo3D_CwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo3D_CwC(numHaloParts, haloSpecs);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_setRemoteHaloInfo1D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo1D_CwG(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo2D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo2D_CwG(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo3D_CwG_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo3D_CwG(numHaloParts, haloSpecs);
}

// GPU with CPU
void tausch_setRemoteHaloInfo1D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo1D_GwC(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo2D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo2D_GwC(numHaloParts, haloSpecs);
}
void tausch_setRemoteHaloInfo3D_GwC_double(CTauschDouble *tC, size_t numHaloParts, TauschHaloSpec *haloSpecs) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->setRemoteHaloInfo3D_GwC(numHaloParts, haloSpecs);
}
#endif


/*********************************************/
// tausch_postReceive*

// CPU with CPU
void tausch_postReceive1D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive1D_CwC(haloId, msgtag);
}
void tausch_postReceive2D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive2D_CwC(haloId, msgtag);
}
void tausch_postReceive3D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive3D_CwC(haloId, msgtag);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_postReceive1D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive1D_CwG(haloId, msgtag);
}
void tausch_postReceive2D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive2D_CwG(haloId, msgtag);
}
void tausch_postReceive3D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive3D_CwG(haloId, msgtag);
}

// GPU with CPU
void tausch_postReceive1D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive1D_GwC(haloId, msgtag);
}
void tausch_postReceive2D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive2D_GwC(haloId, msgtag);
}
void tausch_postReceive3D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postReceive3D_GwC(haloId, msgtag);
}
#endif


/*********************************************/
// postAllReceives*

// CPU with CPU
void tausch_postAllReceives1D_CwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives1D_CwC(msgtag);
}
void tausch_postAllReceives2D_CwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives2D_CwC(msgtag);
}
void tausch_postAllReceives3D_CwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives3D_CwC(msgtag);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_postAllReceives1D_CwG_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives1D_CwG(msgtag);
}
void tausch_postAllReceives2D_CwG_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives2D_CwG(msgtag);
}
void tausch_postAllReceives3D_CwG_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives3D_CwG(msgtag);
}

// GPU with CPU
void tausch_postAllReceives1D_GwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives1D_GwC(msgtag);
}
void tausch_postAllReceives2D_GwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives2D_GwC(msgtag);
}
void tausch_postAllReceives3D_GwC_double(CTauschDouble *tC, int *msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->postAllReceives3D_GwC(msgtag);
}
#endif


/*********************************************/
// tausch_packSendBuffer*

// CPU with CPU
void tausch_packSendBuffer1D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer1D_CwC(haloId, bufferId, buf, region);
}
void tausch_packSendBuffer2D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer2D_CwC(haloId, bufferId, buf, region);
}
void tausch_packSendBuffer3D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer3D_CwC(haloId, bufferId, buf, region);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_packSendBuffer1D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer1D_CwG(haloId, bufferId, buf, region);
}
void tausch_packSendBuffer2D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer2D_CwG(haloId, bufferId, buf, region);
}
void tausch_packSendBuffer3D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer3D_CwG(haloId, bufferId, buf, region);
}

// GPU with CPU
void tausch_packSendBuffer1D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *buf) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer1D_GwC(haloId, bufferId, cl::Buffer(*buf));
}
void tausch_packSendBuffer2D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *buf) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer2D_GwC(haloId, bufferId, cl::Buffer(*buf));
}
void tausch_packSendBuffer3D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *buf) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packSendBuffer3D_GwC(haloId, bufferId, cl::Buffer(*buf));
}
#endif


/*********************************************/
// tausch_send*

// CPU with CPU
void tausch_send1D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send1D_CwC(haloId, msgtag);
}
void tausch_send2D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send2D_CwC(haloId, msgtag);
}
void tausch_send3D_CwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send3D_CwC(haloId, msgtag);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_send1D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send1D_CwG(haloId, msgtag);
}
void tausch_send2D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send2D_CwG(haloId, msgtag);
}
void tausch_send3D_CwG_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send3D_CwG(haloId, msgtag);
}

// GPU with CPU
void tausch_send1D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send1D_GwC(haloId, msgtag);
}
void tausch_send2D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send2D_GwC(haloId, msgtag);
}
void tausch_send3D_GwC_double(CTauschDouble *tC, size_t haloId, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->send3D_GwC(haloId, msgtag);
}
#endif


/*********************************************/
// tausch_recv*

// CPU with CPU
void tausch_recv1D_CwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv1D_CwC(haloId);
}
void tausch_recv2D_CwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv2D_CwC(haloId);
}
void tausch_recv3D_CwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv3D_CwC(haloId);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_recv1D_CwG_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv1D_CwG(haloId);
}
void tausch_recv2D_CwG_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv2D_CwG(haloId);
}
void tausch_recv3D_CwG_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv3D_CwG(haloId);
}

// GPU with CPU
void tausch_recv1D_GwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv1D_GwC(haloId);
}
void tausch_recv2D_GwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv2D_GwC(haloId);
}
void tausch_recv3D_GwC_double(CTauschDouble *tC, size_t haloId) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recv3D_GwC(haloId);
}
#endif


/*********************************************/
// tausch_unpackNextRecvBuffer*

// CPU with CPU
void tausch_unpackNextRecvBuffer1D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer1D_CwC(haloId, bufferId, buf, region);
}
void tausch_unpackNextRecvBuffer2D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer2D_CwC(haloId, bufferId, buf, region);
}
void tausch_unpackNextRecvBuffer3D_CwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer3D_CwC(haloId, bufferId, buf, region);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_unpackNextRecvBuffer1D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer1D_CwG(haloId, bufferId, buf, region);
}
void tausch_unpackNextRecvBuffer2D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer2D_CwG(haloId, bufferId, buf, region);
}
void tausch_unpackNextRecvBuffer3D_CwG_double(CTauschDouble *tC, size_t haloId, size_t bufferId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer3D_CwG(haloId, bufferId, buf, region);
}

// GPU with GPU
void tausch_unpackNextRecvBuffer1D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer1D_GwC(haloId, bufferId, cl::Buffer(*bufcl));
}
void tausch_unpackNextRecvBuffer2D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer2D_GwC(haloId, bufferId, cl::Buffer(*bufcl));
}
void tausch_unpackNextRecvBuffer3D_GwC_double(CTauschDouble *tC, size_t haloId, size_t bufferId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->unpackRecvBuffer3D_GwC(haloId, bufferId, cl::Buffer(*bufcl));
}
#endif


/*********************************************/
// tausch_packAndSend*

// CPU with CPU
void tausch_packAndSend1D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend1D_CwC(haloId, buf, region, msgtag);
}
void tausch_packAndSend2D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend2D_CwC(haloId, buf, region, msgtag);
}
void tausch_packAndSend3D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend3D_CwC(haloId, buf, region, msgtag);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_packAndSend1D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend1D_CwG(haloId, buf, region, msgtag);
}
void tausch_packAndSend2D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend2D_CwG(haloId, buf, region, msgtag);
}
void tausch_packAndSend3D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend3D_CwG(haloId, buf, region, msgtag);
}

// GPU with CPU
void tausch_packAndSend1D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend1D_GwC(haloId, cl::Buffer(*bufcl), msgtag);
}
void tausch_packAndSend2D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend2D_GwC(haloId, cl::Buffer(*bufcl), msgtag);
}
void tausch_packAndSend3D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl, int msgtag) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->packAndSend3D_GwC(haloId, cl::Buffer(*bufcl), msgtag);
}
#endif


/*********************************************/
// tausch_recvAndUnpack*

// CPU with CPU
void tausch_recvAndUnpack1D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack1D_CwC(haloId, buf, region);
}
void tausch_recvAndUnpack2D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack2D_CwC(haloId, buf, region);
}
void tausch_recvAndUnpack3D_CwC_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack3D_CwC(haloId, buf, region);
}

#ifdef TAUSCH_OPENCL
// CPU with GPU
void tausch_recvAndUnpack1D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack1D_CwG(haloId, buf, region);
}
void tausch_recvAndUnpack2D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack2D_CwG(haloId, buf, region);
}
void tausch_recvAndUnpack3D_CwG_double(CTauschDouble *tC, size_t haloId, double *buf, TauschPackRegion region) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack3D_CwG(haloId, buf, region);
}

// GPU with CPU
void tausch_recvAndUnpack1D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack1D_GwC(haloId, cl::Buffer(*bufcl));
}
void tausch_recvAndUnpack2D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack2D_GwC(haloId, cl::Buffer(*bufcl));
}
void tausch_recvAndUnpack3D_GwC_double(CTauschDouble *tC, size_t haloId, cl_mem *bufcl) {
    Tausch<double> *t = reinterpret_cast<Tausch<double>*>(tC);
    t->recvAndUnpack3D_GwC(haloId, cl::Buffer(*bufcl));
}
#endif

#ifdef __cplusplus
}
#endif
