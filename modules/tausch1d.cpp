#include "tausch1d.h"

template <class buf_t> Tausch1D<buf_t>::Tausch1D(MPI_Datatype mpiDataType,
                                                   size_t numBuffers, size_t *valuesPerPointPerBuffer, MPI_Comm comm) {

    MPI_Comm_dup(comm, &TAUSCH_COMM);

    // get MPI info
    MPI_Comm_rank(TAUSCH_COMM, &mpiRank);
    MPI_Comm_size(TAUSCH_COMM, &mpiSize);

    this->numBuffers = numBuffers;

    this->valuesPerPointPerBuffer = new size_t[numBuffers];
    if(valuesPerPointPerBuffer == nullptr)
        for(int i = 0; i < numBuffers; ++i)
            this->valuesPerPointPerBuffer[i] = 1;
    else
        for(int i = 0; i < numBuffers; ++i)
            this->valuesPerPointPerBuffer[i] = valuesPerPointPerBuffer[i];

    this->mpiDataType = mpiDataType;

}

template <class buf_t> Tausch1D<buf_t>::~Tausch1D() {
    for(int i = 0; i < localHaloNumParts; ++i)
        delete[] mpiSendBuffer[i];
    for(int i = 0; i < remoteHaloNumParts; ++i)
        delete[] mpiRecvBuffer[i];
    delete[] localHaloSpecs;
    delete[] mpiSendBuffer;
    delete[] remoteHaloSpecs;
    delete[] mpiRecvBuffer;

    delete[] mpiSendRequests;
    delete[] mpiRecvRequests;

    delete[] setupMpiRecv;
    delete[] setupMpiSend;

    delete[] valuesPerPointPerBuffer;
}

template <class buf_t> void Tausch1D<buf_t>::setLocalHaloInfoCpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumParts = numHaloParts;
    localHaloSpecs = new TauschHaloSpec[numHaloParts];
    mpiSendBuffer = new buf_t*[numHaloParts];
    mpiSendRequests = new MPI_Request[numHaloParts];
    setupMpiSend = new bool[numHaloParts];
    setupMpiRecv = new bool[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecs[i].bufferWidth = haloSpecs[i].bufferWidth;
        localHaloSpecs[i].haloX = haloSpecs[i].haloX;
        localHaloSpecs[i].haloWidth = haloSpecs[i].haloWidth;
        localHaloSpecs[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        mpiSendBuffer[i] = new buf_t[bufsize]{};

        setupMpiSend[i] = false;

    }

}

template <class buf_t> void Tausch1D<buf_t>::setRemoteHaloInfoCpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumParts = numHaloParts;
    remoteHaloSpecs = new TauschHaloSpec[numHaloParts];
    mpiRecvBuffer = new buf_t*[numHaloParts];
    mpiRecvRequests = new MPI_Request[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecs[i].bufferWidth = haloSpecs[i].bufferWidth;
        remoteHaloSpecs[i].haloX = haloSpecs[i].haloX;
        remoteHaloSpecs[i].haloWidth = haloSpecs[i].haloWidth;
        remoteHaloSpecs[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        mpiRecvBuffer[i] = new buf_t[bufsize]{};

        setupMpiRecv[i] = false;

    }

}

template <class buf_t> void Tausch1D<buf_t>::postReceiveCpu(size_t haloId, int mpitag) {

    if(!setupMpiRecv[haloId]) {

        if(mpitag == -1) {
            std::cerr << "[Tausch1D] ERROR: MPI_Recv for halo region #" << haloId << " hasn't been posted before, missing mpitag... Abort!" << std::endl;
            exit(1);
        }

        setupMpiRecv[haloId] = true;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecs[haloId].haloWidth;

        MPI_Recv_init(&mpiRecvBuffer[haloId][0], bufsize, mpiDataType, remoteHaloSpecs[haloId].remoteMpiRank,
                      mpitag, TAUSCH_COMM, &mpiRecvRequests[haloId]);

    }

    MPI_Start(&mpiRecvRequests[haloId]);

}

template <class buf_t> void Tausch1D<buf_t>::postAllReceivesCpu(int *mpitag) {

    if(mpitag == nullptr) {
        mpitag = new int[remoteHaloNumParts];
        for(int id = 0; id < remoteHaloNumParts; ++id)
            mpitag[id] = -1;
    }

    for(int id = 0; id < remoteHaloNumParts; ++id)
        postReceiveCpu(id,mpitag[id]);

}

template <class buf_t> void Tausch1D<buf_t>::packSendBufferCpu(size_t haloId, size_t bufferId, buf_t *buf, TauschPackRegion region) {

    for(int s = 0; s < region.width; ++s) {
        int bufIndex = localHaloSpecs[haloId].haloX + s + region.x;
        int mpiIndex = s + region.x;
        for(int val = 0; val < valuesPerPointPerBuffer[bufferId]; ++val) {
            int offset = 0;
            for(int b = 0; b < bufferId; ++b)
                offset += valuesPerPointPerBuffer[b]*localHaloSpecs[haloId].haloWidth;
            mpiSendBuffer[haloId][offset + valuesPerPointPerBuffer[bufferId]*mpiIndex + val] =
                    buf[valuesPerPointPerBuffer[bufferId]*bufIndex + val];
        }
    }

}

template <class buf_t> void Tausch1D<buf_t>::packSendBufferCpu(size_t haloId, size_t bufferId, buf_t *buf) {
    TauschPackRegion region;
    region.x = 0;
    region.width = localHaloSpecs[haloId].haloWidth;
    packSendBufferCpu(haloId, bufferId, buf, region);
}

template <class buf_t> void Tausch1D<buf_t>::sendCpu(size_t haloId, int mpitag) {

    if(!setupMpiSend[haloId]) {

        if(mpitag == -1) {
            std::cerr << "[Tausch1D] ERROR: MPI_Send for halo region #" << haloId << " hasn't been posted before, missing mpitag... Abort!" << std::endl;
            exit(1);
        }

        setupMpiSend[haloId] = true;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*localHaloSpecs[haloId].haloWidth;

        MPI_Send_init(&mpiSendBuffer[haloId][0], bufsize, mpiDataType, localHaloSpecs[haloId].remoteMpiRank,
                      mpitag, TAUSCH_COMM, &mpiSendRequests[haloId]);

    }

    MPI_Start(&mpiSendRequests[haloId]);

}

template <class buf_t> void Tausch1D<buf_t>::recvCpu(size_t haloId) {
    MPI_Wait(&mpiRecvRequests[haloId], MPI_STATUS_IGNORE);
}

template <class buf_t> void Tausch1D<buf_t>::unpackRecvBufferCpu(size_t haloId, size_t bufferId, buf_t *buf, TauschPackRegion region) {

    for(int s = 0; s < region.width; ++s) {
        int bufIndex = remoteHaloSpecs[haloId].haloX + s + region.x;
        int mpiIndex = s + region.x;
        for(int val = 0; val < valuesPerPointPerBuffer[bufferId]; ++val) {
            int offset = 0;
            for(int b = 0; b < bufferId; ++b)
                offset += valuesPerPointPerBuffer[b]*remoteHaloSpecs[haloId].haloWidth;
            buf[valuesPerPointPerBuffer[bufferId]*bufIndex + val] =
                    mpiRecvBuffer[haloId][offset + valuesPerPointPerBuffer[bufferId]*mpiIndex + val];
        }
    }

}

template <class buf_t> void Tausch1D<buf_t>::unpackRecvBufferCpu(size_t haloId, size_t bufferId, buf_t *buf) {
    TauschPackRegion region;
    region.x = 0;
    region.width = remoteHaloSpecs[haloId].haloWidth;
    unpackRecvBufferCpu(haloId, bufferId, buf, region);
}

template <class buf_t> void Tausch1D<buf_t>::packAndSendCpu(size_t haloId, buf_t *buf, TauschPackRegion region, int mpitag) {
    packSendBufferCpu(haloId, 0, buf, region);
    sendCpu(haloId, mpitag);
}

template <class buf_t> void Tausch1D<buf_t>::packAndSendCpu(size_t haloId, buf_t *buf, int mpitag) {
    TauschPackRegion region;
    region.x = 0;
    region.width = localHaloSpecs[haloId].haloWidth;
    packSendBufferCpu(haloId, 0, buf, region);
    sendCpu(haloId, mpitag);
}

template <class buf_t> void Tausch1D<buf_t>::recvAndUnpackCpu(size_t haloId, buf_t *buf, TauschPackRegion region) {
    recvCpu(haloId);
    unpackRecvBufferCpu(haloId, 0, buf, region);
}

template <class buf_t> void Tausch1D<buf_t>::recvAndUnpackCpu(size_t haloId, buf_t *buf) {
    TauschPackRegion region;
    region.x = 0;
    region.width = remoteHaloSpecs[haloId].haloWidth;
    recvCpu(haloId);
    unpackRecvBufferCpu(haloId, 0, buf, region);
}


#ifdef TAUSCH_OPENCL

template <class buf_t> void Tausch1D<buf_t>::enableOpenCL(bool blockingSyncCpuGpu, int clLocalWorkgroupSize, bool giveOpenCLDeviceName, bool showOpenCLBuildLog) {

    this->blockingSyncCpuGpu = blockingSyncCpuGpu;
    cl_kernelLocalSize = clLocalWorkgroupSize;
    this->showOpenCLBuildLog = showOpenCLBuildLog;

    sync_counter[0].store(0); sync_counter[1].store(0);
    sync_lock[0].store(0); sync_lock[1].store(0);

    setupOpenCL(giveOpenCLDeviceName);

    try {
        cl_valuesPerPointPerBuffer = cl::Buffer(cl_context, &valuesPerPointPerBuffer[0], &valuesPerPointPerBuffer[numBuffers], true);
    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: enableOpenCL() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch1D<buf_t>::setLocalHaloInfoCpuForGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumPartsCpuForGpu = numHaloParts;
    localHaloSpecsCpuForGpu = new TauschHaloSpec[numHaloParts];
    cpuToGpuSendBuffer = new std::atomic<buf_t>*[numHaloParts];
    msgtagsCpuToGpu = new std::atomic<int>[numHaloParts]{};

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecsCpuForGpu[i].bufferWidth = haloSpecs[i].bufferWidth;
        localHaloSpecsCpuForGpu[i].haloX = haloSpecs[i].haloX;
        localHaloSpecsCpuForGpu[i].haloWidth = haloSpecs[i].haloWidth;
        localHaloSpecsCpuForGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        cpuToGpuSendBuffer[i] = new std::atomic<buf_t>[bufsize]{};

    }

}

template <class buf_t> void Tausch1D<buf_t>::setRemoteHaloInfoCpuForGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumPartsCpuForGpu = numHaloParts;
    remoteHaloSpecsCpuForGpu = new TauschHaloSpec[numHaloParts];
    gpuToCpuRecvBuffer = new buf_t*[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecsCpuForGpu[i].bufferWidth = haloSpecs[i].bufferWidth;
        remoteHaloSpecsCpuForGpu[i].haloX = haloSpecs[i].haloX;
        remoteHaloSpecsCpuForGpu[i].haloWidth = haloSpecs[i].haloWidth;
        remoteHaloSpecsCpuForGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        gpuToCpuRecvBuffer[i] = new buf_t[bufsize];

    }

}

template <class buf_t> void Tausch1D<buf_t>::setLocalHaloInfoGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumPartsGpu = numHaloParts;
    localHaloSpecsGpu = new TauschHaloSpec[numHaloParts];
    gpuToCpuSendBuffer = new std::atomic<buf_t>*[numHaloParts];
    msgtagsGpuToCpu = new std::atomic<int>[numHaloParts]{};

    try {
        cl_gpuToCpuSendBuffer = new cl::Buffer[numHaloParts];
        cl_localHaloSpecsGpu = new cl::Buffer[numHaloParts];
    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: setLocalHaloInfoGpu() (1) :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecsGpu[i].bufferWidth = haloSpecs[i].bufferWidth;
        localHaloSpecsGpu[i].haloX = haloSpecs[i].haloX;
        localHaloSpecsGpu[i].haloWidth = haloSpecs[i].haloWidth;
        localHaloSpecsGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        gpuToCpuSendBuffer[i] = new std::atomic<buf_t>[bufsize]{};

        size_t tmpHaloSpecs[3] = {haloSpecs[i].haloX, haloSpecs[i].haloWidth, haloSpecs[i].bufferWidth};

        try {
            cl_gpuToCpuSendBuffer[i] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, bufsize*sizeof(buf_t));
            cl_localHaloSpecsGpu[i] = cl::Buffer(cl_context, &tmpHaloSpecs[0], &tmpHaloSpecs[3], true);
        } catch(cl::Error error) {
            std::cerr << "Tausch1D :: setLocalHaloInfoGpu() (2) :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
            exit(1);
        }

    }

}

template <class buf_t> void Tausch1D<buf_t>::setRemoteHaloInfoGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumPartsGpu = numHaloParts;
    remoteHaloSpecsGpu = new TauschHaloSpec[numHaloParts];
    cpuToGpuRecvBuffer = new buf_t*[numHaloParts];

    try {
        cl_cpuToGpuRecvBuffer = new cl::Buffer[remoteHaloNumPartsGpu];
        cl_remoteHaloSpecsGpu = new cl::Buffer[numHaloParts];
    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: enableOpenCL() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecsGpu[i].bufferWidth = haloSpecs[i].bufferWidth;
        remoteHaloSpecsGpu[i].haloX = haloSpecs[i].haloX;
        remoteHaloSpecsGpu[i].haloWidth = haloSpecs[i].haloWidth;
        remoteHaloSpecsGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].haloWidth;
        cpuToGpuRecvBuffer[i] = new buf_t[bufsize];

        size_t tmpHaloSpecs[3] = {haloSpecs[i].haloX, haloSpecs[i].haloWidth, haloSpecs[i].bufferWidth};

        try {
            cl_cpuToGpuRecvBuffer[i] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, bufsize*sizeof(double));
            cl_remoteHaloSpecsGpu[i] = cl::Buffer(cl_context, &tmpHaloSpecs[0], &tmpHaloSpecs[3], true);
        } catch(cl::Error error) {
            std::cerr << "Tausch1D :: setRemoteHaloInfo() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
            exit(1);
        }

    }

}

template <class buf_t> void Tausch1D<buf_t>::packSendBufferCpuToGpu(size_t haloId, size_t bufferId, buf_t *buf, TauschPackRegion region) {

    for(int s = 0; s < region.width; ++s) {
        int bufIndex = s+localHaloSpecsCpuForGpu[haloId].haloX + region.x;
        int mpiIndex = s + region.x;
        for(int val = 0; val < valuesPerPointPerBuffer[bufferId]; ++val) {
            int offset = 0;
            for(int b = 0; b < bufferId; ++b)
                offset += valuesPerPointPerBuffer[b]*localHaloSpecsCpuForGpu[haloId].haloWidth;
            cpuToGpuSendBuffer[haloId][offset + valuesPerPointPerBuffer[bufferId]*mpiIndex + val].store(buf[valuesPerPointPerBuffer[bufferId]*bufIndex + val]);
        }
    }

}

template <class buf_t> void Tausch1D<buf_t>::packSendBufferCpuToGpu(size_t haloId, size_t bufferId, buf_t *buf) {

    TauschPackRegion region;
    region.x = 0;
    region.width = localHaloSpecsCpuForGpu[haloId].haloWidth;

    packSendBufferCpuToGpu(haloId, bufferId, buf, region);

}

template <class buf_t> void Tausch1D<buf_t>::packSendBufferGpuToCpu(size_t haloId, size_t bufferId, cl::Buffer buf) {

    try {

        auto kernel_packNextSendBuffer = cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer>
                                                    (cl_programs, "packSendBuffer");

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*localHaloSpecsGpu[haloId].haloWidth;

        int globalsize = (bufsize/cl_kernelLocalSize +1)*cl_kernelLocalSize;

        cl::Buffer cl_bufferId(cl_context, &bufferId, (&bufferId)+1, true);

        kernel_packNextSendBuffer(cl::EnqueueArgs(cl_queue, cl::NDRange(globalsize), cl::NDRange(cl_kernelLocalSize)),
                                  cl_localHaloSpecsGpu[haloId], cl_valuesPerPointPerBuffer,
                                  cl_bufferId, cl_gpuToCpuSendBuffer[haloId], buf);

    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch1D<buf_t>::sendCpuToGpu(size_t haloId, int msgtag) {
    msgtagsCpuToGpu[haloId].store(msgtag);
    syncCpuAndGpu();
}

template <class buf_t> void Tausch1D<buf_t>::sendGpuToCpu(size_t haloId, int msgtag) {

    msgtagsGpuToCpu[haloId].store(msgtag);

    try {

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*localHaloSpecsGpu[haloId].haloWidth;

        buf_t *tmp = new buf_t[bufsize];
        cl::copy(cl_queue, cl_gpuToCpuSendBuffer[haloId], &tmp[0], &tmp[bufsize]);
        for(int i = 0; i < bufsize; ++i)
            gpuToCpuSendBuffer[haloId][i].store(tmp[i]);

    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: sendGpuToCpu() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    syncCpuAndGpu();

}

template <class buf_t> void Tausch1D<buf_t>::recvCpuToGpu(size_t haloId, int msgtag) {

    syncCpuAndGpu();

    int remoteid = obtainRemoteId(msgtag);

    size_t bufsize = 0;
    for(int n = 0; n < numBuffers; ++n)
        bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsGpu[haloId].haloWidth;

    for(int j = 0; j < bufsize; ++j)
        cpuToGpuRecvBuffer[haloId][j] = cpuToGpuSendBuffer[remoteid][j].load();

    try {
        cl_cpuToGpuRecvBuffer[haloId] = cl::Buffer(cl_context, &cpuToGpuRecvBuffer[haloId][0], &cpuToGpuRecvBuffer[haloId][bufsize], false);
    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: recvCpuToGpu() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch1D<buf_t>::recvGpuToCpu(size_t haloId, int msgtag) {

    syncCpuAndGpu();

    int remoteid = obtainRemoteId(msgtag);

    size_t bufsize = 0;
    for(int n = 0; n < numBuffers; ++n)
        bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsCpuForGpu[haloId].haloWidth;
    for(int i = 0; i < bufsize; ++i)
        gpuToCpuRecvBuffer[haloId][i] = gpuToCpuSendBuffer[remoteid][i].load();

}

template <class buf_t> void Tausch1D<buf_t>::unpackRecvBufferGpuToCpu(size_t haloId, size_t bufferId, buf_t *buf, TauschPackRegion region) {

    for(int s = 0; s < region.width; ++s) {
        int bufIndex = s+remoteHaloSpecsCpuForGpu[haloId].haloX + region.x;
        int mpiIndex = s + region.x;
        for(int val = 0; val < valuesPerPointPerBuffer[bufferId]; ++val) {
            int offset = 0;
            for(int b = 0; b < bufferId; ++b)
                offset += valuesPerPointPerBuffer[b]*remoteHaloSpecsCpuForGpu[haloId].haloWidth;
            buf[valuesPerPointPerBuffer[bufferId]*bufIndex + val] = gpuToCpuRecvBuffer[haloId][offset + valuesPerPointPerBuffer[bufferId]*mpiIndex + val];
        }
    }

}

template <class buf_t> void Tausch1D<buf_t>::unpackRecvBufferGpuToCpu(size_t haloId, size_t bufferId, buf_t *buf) {

    TauschPackRegion region;
    region.x = 0;
    region.width = remoteHaloSpecsCpuForGpu[haloId].haloWidth;

    unpackRecvBufferGpuToCpu(haloId, bufferId, buf, region);

}

template <class buf_t> void Tausch1D<buf_t>::unpackRecvBufferCpuToGpu(size_t haloId, size_t bufferId, cl::Buffer buf) {

    try {
        auto kernel_unpackRecvBuffer = cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer>
                                                (cl_programs, "unpackRecvBuffer");

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsGpu[haloId].haloWidth;

        int globalsize = (bufsize/cl_kernelLocalSize +1)*cl_kernelLocalSize;

        cl::Buffer cl_bufferId(cl_context, &bufferId, (&bufferId)+1, true);

        kernel_unpackRecvBuffer(cl::EnqueueArgs(cl_queue, cl::NDRange(globalsize), cl::NDRange(cl_kernelLocalSize)),
                                cl_remoteHaloSpecsGpu[haloId], cl_valuesPerPointPerBuffer, cl_bufferId,
                                cl_cpuToGpuRecvBuffer[haloId], buf);

    } catch(cl::Error error) {
        std::cerr << "Tausch1D :: unpackRecvBufferCpuToGpu() :: OpenCL exception caught: " << error.what()
                  << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch1D<buf_t>::syncCpuAndGpu() {

    // need to do this twice to prevent potential (though unlikely) deadlocks
    for(int i = 0; i < 2; ++i) {

        if(sync_lock[i].load() == 0)
            sync_lock[i].store(1);
        int val = sync_counter[i].fetch_add(1);
        if(val == 1) {
            sync_counter[i].store(0);
            sync_lock[i].store(0);
        }
        while(sync_lock[i].load() == 1);

    }

}

template <class buf_t> int Tausch1D<buf_t>::obtainRemoteId(int msgtag) {
    for(int j = 0; j < remoteHaloNumPartsCpuForGpu; ++j) {
        if(msgtagsCpuToGpu[j].load() == msgtag)
            return j;
    }
    return 0;
}

template <class buf_t> void Tausch1D<buf_t>::setupOpenCL(bool giveOpenCLDeviceName) {

    try {

        // Get platform count
        std::vector<cl::Platform> all_platforms;
        cl::Platform::get(&all_platforms);
        int platform_length = all_platforms.size();

        // We need at most mpiSize many devices
        int *platform_num = new int[mpiSize]{};
        int *device_num = new int[mpiSize]{};

        // Counter so that we know when to stop
        int num = 0;

        // Loop over platforms
        for(int i = 0; i < platform_length; ++i) {
            // Get devices on platform
            std::vector<cl::Device> all_devices;
            all_platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
            int device_length = all_devices.size();
            // Loop over platforms
            for(int j = 0; j < device_length; ++j) {
                // Store current pair
                platform_num[num] = i;
                device_num[num] = j;
                ++num;
                // and stop
                if(num == mpiSize) {
                    i = platform_length;
                    break;
                }
            }
        }

        // Get the platform and device to be used by this MPI thread
        cl_platform = all_platforms[platform_num[mpiRank%num]];
        std::vector<cl::Device> all_devices;
        cl_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
        cl_defaultDevice = all_devices[device_num[mpiRank%num]];

        // Give some feedback of the choice.
        if(giveOpenCLDeviceName) {
            for(int iRank = 0; iRank < mpiSize; ++iRank){
                if(mpiRank == iRank)
                    std::cout << "Rank " << mpiRank << " using OpenCL platform #" << platform_num[mpiRank%num]
                              << " with device #" << device_num[mpiRank%num] << ": " << cl_defaultDevice.getInfo<CL_DEVICE_NAME>() << std::endl;
                MPI_Barrier(TAUSCH_COMM);
            }
            if(mpiRank == 0)
                std::cout << std::endl;
        }

        delete[] platform_num;
        delete[] device_num;

        // Create context and queue
        cl_context = cl::Context({cl_defaultDevice});
        cl_queue = cl::CommandQueue(cl_context,cl_defaultDevice);

    } catch(cl::Error error) {
        std::cout << "[setup] OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    // And compile kernels
    compileKernels();

}

template <class buf_t> void Tausch1D<buf_t>::compileKernels() {

  std::string oclstr = R"d(
kernel void packSendBuffer(global const size_t * restrict const haloSpecs,
                           global const size_t * restrict const valuesPerPointPerBuffer, global int * restrict const bufferId,
                           global double * restrict const haloBuffer, global const double * restrict const buffer) {

    const int current = get_global_id(0);

    int maxSize = haloSpecs[1];

    if(current >= maxSize) return;

    int index = current + haloSpecs[0];

    for(int val = 0; val < valuesPerPointPerBuffer[*bufferId]; ++val) {
        int offset = 0;
        for(int b = 0; b < *bufferId; ++b)
            offset += valuesPerPointPerBuffer[b]*maxSize;
        haloBuffer[offset+ valuesPerPointPerBuffer[*bufferId]*current + val] = buffer[valuesPerPointPerBuffer[*bufferId]*index + val];
    }

}

kernel void unpackRecvBuffer(global const size_t * restrict const haloSpecs,
                             global const size_t * restrict const valuesPerPointPerBuffer, global int * restrict const bufferId,
                             global const double * restrict const haloBuffer, global double * restrict const buffer) {

    const int current = get_global_id(0);

    int maxSize = haloSpecs[1];

    if(current >= maxSize) return;

    int index = current + haloSpecs[0];

    for(int val = 0; val < valuesPerPointPerBuffer[*bufferId]; ++val) {
        int offset = 0;
        for(int b = 0; b < *bufferId; ++b)
            offset += valuesPerPointPerBuffer[b]*maxSize;
        buffer[valuesPerPointPerBuffer[*bufferId]*index + val] =
                haloBuffer[offset + valuesPerPointPerBuffer[*bufferId]*current + val];
    }

}
      )d";

    try {
        cl_programs = cl::Program(cl_context, oclstr, false);
        cl_programs.build("");
        if(showOpenCLBuildLog) {
            try {
                std::string log = cl_programs.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl_defaultDevice);
                std::cout << std::endl << " ******************** " << std::endl << " ** BUILD LOG" << std::endl
                          << " ******************** " << std::endl << log << std::endl << std::endl << " ******************** "
                          << std::endl << std::endl;
            } catch(cl::Error err) {
                std::cout << "Tausch1D :: compileKernels() :: getBuildInfo :: OpenCL exception caught: " << err.what() << " (" << err.err() << ")" << std::endl;
            }
        }
    } catch(cl::Error error) {
        std::cout << "Tausch1D :: compileKernels() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        if(error.err() == -11) {
            try {
                std::string log = cl_programs.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl_defaultDevice);
                std::cout << std::endl << " ******************** " << std::endl << " ** BUILD LOG" << std::endl
                          << " ******************** " << std::endl << log << std::endl << std::endl << " ******************** "
                          << std::endl << std::endl;
            } catch(cl::Error err) {
                std::cout << "Tausch1D :: compileKernels() :: getBuildInfo :: OpenCL exception caught: " << err.what() << " (" << err.err() << ")" << std::endl;
            }
        }
    }

}


#endif

template class Tausch1D<char>;
template class Tausch1D<char16_t>;
template class Tausch1D<char32_t>;
template class Tausch1D<wchar_t>;
template class Tausch1D<signed char>;
template class Tausch1D<short int>;
template class Tausch1D<int>;
template class Tausch1D<long>;
template class Tausch1D<long long>;
template class Tausch1D<unsigned char>;
template class Tausch1D<unsigned short int>;
template class Tausch1D<unsigned int>;
template class Tausch1D<unsigned long>;
template class Tausch1D<unsigned long long>;
template class Tausch1D<float>;
template class Tausch1D<double>;
template class Tausch1D<long double>;
template class Tausch1D<bool>;
