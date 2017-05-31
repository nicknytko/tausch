#include "../tausch.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "ctauschint.h"

CTauschInt *tausch_new_int(int *localDim, int *haloWidth, int numBuffers, int valuesPerPoint, MPI_Comm comm, TauschVersion version) {

    if(version != TAUSCH_1D && version != TAUSCH_2D && version != TAUSCH_3D) {
        std::cerr << "[CTauschInt] ERROR: Invalid version specified: " << version << " - Abort..." << std::endl;
        exit(1);
    }

    Tausch<int> *t;

    if(version == TAUSCH_1D)
        t = new Tausch1D<int>(localDim, haloWidth, numBuffers, valuesPerPoint, comm);
    else if(version == TAUSCH_2D)
        t = new Tausch2D<int>(localDim, haloWidth, numBuffers, valuesPerPoint, comm);
    else if(version == TAUSCH_3D)
        t = new Tausch3D<int>(localDim, haloWidth, numBuffers, valuesPerPoint, comm);

    return reinterpret_cast<CTauschInt*>(t);

}

void tausch_delete_int(CTauschInt *tC) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    delete t;
}

void tausch_setCpuLocalHaloInfo_int(CTauschInt *tC, int numHaloParts, int **haloSpecs) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->setCpuLocalHaloInfo(numHaloParts, haloSpecs);
}

void tausch_setCpuRemoteHaloInfo_int(CTauschInt *tC, int numHaloParts, int **haloSpecs) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->setCpuRemoteHaloInfo(numHaloParts, haloSpecs);
}

void tausch_postMpiReceives_int(CTauschInt *tC) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->postMpiReceives();
}

void tausch_packNextSendBuffer_int(CTauschInt *tC, int id, int *buf) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->packNextSendBuffer(id, buf);
}

void tausch_send_int(CTauschInt *tC, int id) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->send(id);
}

void tausch_recv_int(CTauschInt *tC, int id) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->recv(id);
}

void tausch_unpackNextRecvBuffer_int(CTauschInt *tC, int id, int *buf) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->unpackNextRecvBuffer(id, buf);
}

void tausch_packAndSend_int(CTauschInt *tC, int id, int *buf) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->packAndSend(id, buf);
}

void tausch_recvAndUnpack_int(CTauschInt *tC, int id, int *buf) {
    Tausch<int> *t = reinterpret_cast<Tausch<int>*>(tC);
    t->recvAndUnpack(id, buf);
}


#ifdef __cplusplus
}
#endif

