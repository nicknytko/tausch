#include <tausch/tausch.h>
#include <cmath>
#include <chrono>

int main(int argc, char** argv) {

    // Initialize MPI.
    int provided;
    MPI_Init_thread(&argc,&argv,MPI_THREAD_SERIALIZED,&provided);

    // Get MPI metadata
    int mpiRank = 0, mpiSize = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);

    // The local dimensions of the domain, with the halowidths added on
    size_t localDim[2];
    localDim[TAUSCH_X] = 100;
    localDim[TAUSCH_Y] = 120;

    // The layout of the MPI ranks. If mpiSize is a perfect square we take its square root for each dimension,
    // otherwise all MPI ranks are lined up in the x direction
    size_t mpiNum[2];
    mpiNum[TAUSCH_X] = std::sqrt(mpiSize);
    mpiNum[TAUSCH_Y] = std::sqrt(mpiSize);
    if(mpiNum[TAUSCH_X]*mpiNum[TAUSCH_Y] != mpiSize) {
        mpiNum[TAUSCH_X] = mpiSize;
        mpiNum[TAUSCH_Y] = 1;
    }

    // The four MPI rank neighbours we use for this example
    size_t left, right;
    // If we are along a domain boundary, we wrap around to the opposite end (periodic)
    if(mpiRank%mpiNum[TAUSCH_X] == 0)
        left = mpiRank+mpiNum[TAUSCH_X]-1;
    else
        left = mpiRank-1;
    if((mpiRank+1)%mpiNum[TAUSCH_X] == 0)
        right = mpiRank-mpiNum[TAUSCH_X]+1;
    else
        right = mpiRank+1;

    //. Here we will use two buffers that require a halo exchange
    size_t numBuffers = 2;
    double *dat1 = new double[localDim[TAUSCH_X]*localDim[TAUSCH_Y]]{};
    double *dat2 = new double[localDim[TAUSCH_X]*localDim[TAUSCH_Y]]{};

    // We have four halo regions, one across each of the four edges
    TauschHaloSpec remoteHaloSpecs;
    TauschHaloSpec localHaloSpecs;

    // left edge (0) remote halo region: [x, y, w, h, receiver, tag]
    remoteHaloSpecs.haloX = 0;
    remoteHaloSpecs.haloY = 0;
    remoteHaloSpecs.haloWidth = 1;
    remoteHaloSpecs.haloHeight = localDim[TAUSCH_Y];
    remoteHaloSpecs.bufferWidth = localDim[TAUSCH_X];
    remoteHaloSpecs.bufferHeight = localDim[TAUSCH_Y];
    remoteHaloSpecs.remoteMpiRank = left;

    // right edge (1) local halo region: [x, y, w, h, sender, tag]
    localHaloSpecs.haloX = localDim[TAUSCH_X];
    localHaloSpecs.haloY = 0;
    localHaloSpecs.haloWidth = 1;
    localHaloSpecs.haloHeight = localDim[TAUSCH_Y];
    localHaloSpecs.bufferWidth = localDim[TAUSCH_X];
    localHaloSpecs.bufferHeight = localDim[TAUSCH_Y];
    localHaloSpecs.remoteMpiRank = right;

    // The Tausch object, using its double version. The pointer type is of type 'Tausch', although using Tausch2D directly would also be possible here
    Tausch<double> *tausch = new Tausch<double>(MPI_DOUBLE, numBuffers, nullptr, MPI_COMM_WORLD);

    // Tell Tausch about the local and remote halo regions
    tausch->setLocalHaloInfo2D_CwC(1, &localHaloSpecs);
    tausch->setRemoteHaloInfo2D_CwC(1, &remoteHaloSpecs);

    /*****************
     * HALO EXCHANGE *
     *****************/

    MPI_Barrier(MPI_COMM_WORLD);

    // Start a timer
    auto t_start = std::chrono::steady_clock::now();

    // We only send one message and receive one message, all with MPI tag 0
    int mpitag = 0;

    // post the MPI receives
    tausch->postAllReceives2D_CwC(&mpitag);

    // pack the right buffers and send them off
    tausch->packSendBuffer2D_CwC(0, 0, dat1);
    tausch->packSendBuffer2D_CwC(0, 1, dat2);
    tausch->send2D_CwC(0, mpitag);

    // receive the left buffers and unpack them
    tausch->recv2D_CwC(0);
    tausch->unpackRecvBuffer2D_CwC(0, 0, dat1);
    tausch->unpackRecvBuffer2D_CwC(0, 1, dat2);

    MPI_Barrier(MPI_COMM_WORLD);

    /*****************/

    // End timer
    auto t_end = std::chrono::steady_clock::now();

    // Output timing result
    if(mpiRank == 0)
        std::cout << "Required time: " << std::chrono::duration<double, std::milli>(t_end-t_start).count() << " ms" << std::endl;

    // Clean up memory
    delete[] dat1;
    delete[] dat2;
    delete tausch;

    MPI_Finalize();
    return 0;
}