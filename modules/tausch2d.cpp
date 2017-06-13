#include "tausch2d.h"

template <class buf_t> Tausch2D<buf_t>::Tausch2D(size_t *localDim, MPI_Datatype mpiDataType,
                                                 size_t numBuffers, size_t *valuesPerPointPerBuffer, MPI_Comm comm) {

    MPI_Comm_dup(comm, &TAUSCH_COMM);

    // get MPI info
    MPI_Comm_rank(TAUSCH_COMM, &mpiRank);
    MPI_Comm_size(TAUSCH_COMM, &mpiSize);

    this->localDim[0] = localDim[0];
    this->localDim[1] = localDim[1];

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

template <class buf_t> Tausch2D<buf_t>::~Tausch2D() {
    for(int i = 0; i < localHaloNumPartsCpu; ++i)
        delete[] mpiSendBuffer[i];
    for(int i = 0; i < remoteHaloNumPartsCpu; ++i)
        delete[] mpiRecvBuffer[i];
    delete[] localHaloSpecsCpu;
    delete[] mpiSendBuffer;
    delete[] remoteHaloSpecsCpu;
    delete[] mpiRecvBuffer;

    delete[] mpiSendRequests;
    delete[] numBuffersPackedCpu;
    delete[] mpiRecvRequests;
    delete[] numBuffersUnpackedCpu;

    delete[] setupMpiRecv;
    delete[] setupMpiSend;

    delete[] valuesPerPointPerBuffer;
}

template <class buf_t> void Tausch2D<buf_t>::setLocalHaloInfoCpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumPartsCpu = numHaloParts;
    localHaloSpecsCpu = new TauschHaloSpec[numHaloParts];
    mpiSendBuffer = new buf_t*[numHaloParts];
    mpiSendRequests = new MPI_Request[numHaloParts];
    numBuffersPackedCpu =  new size_t[numHaloParts]{};
    setupMpiSend = new bool[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecsCpu[i].x = haloSpecs[i].x;
        localHaloSpecsCpu[i].y = haloSpecs[i].y;
        localHaloSpecsCpu[i].width = haloSpecs[i].width;
        localHaloSpecsCpu[i].height = haloSpecs[i].height;
        localHaloSpecsCpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        mpiSendBuffer[i] = new buf_t[bufsize]{};

        setupMpiSend[i] = false;

    }

}

template <class buf_t> void Tausch2D<buf_t>::setRemoteHaloInfoCpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumPartsCpu = numHaloParts;
    remoteHaloSpecsCpu = new TauschHaloSpec[numHaloParts];
    mpiRecvBuffer = new buf_t*[numHaloParts];
    mpiRecvRequests = new MPI_Request[numHaloParts];
    numBuffersUnpackedCpu =  new size_t[numHaloParts]{};
    setupMpiRecv = new bool[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecsCpu[i].x = haloSpecs[i].x;
        remoteHaloSpecsCpu[i].y = haloSpecs[i].y;
        remoteHaloSpecsCpu[i].width = haloSpecs[i].width;
        remoteHaloSpecsCpu[i].height = haloSpecs[i].height;
        remoteHaloSpecsCpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        mpiRecvBuffer[i] = new buf_t[bufsize]{};

        setupMpiRecv[i] = false;

    }

}

template <class buf_t> void Tausch2D<buf_t>::postReceiveCpu(size_t id, int mpitag) {

    if(!setupMpiRecv[id]) {

        if(mpitag == -1) {
            std::cerr << "[Tausch2D] ERROR: MPI_Recv for halo region #" << id << " hasn't been posted before, missing mpitag... Abort!" << std::endl;
            exit(1);
        }

        setupMpiRecv[id] = true;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsCpu[id].width*remoteHaloSpecsCpu[id].height;

        MPI_Recv_init(&mpiRecvBuffer[id][0], bufsize, mpiDataType,
                      remoteHaloSpecsCpu[id].remoteMpiRank, mpitag, TAUSCH_COMM, &mpiRecvRequests[id]);

    }

    MPI_Start(&mpiRecvRequests[id]);

}

template <class buf_t> void Tausch2D<buf_t>::postAllReceivesCpu(int *mpitag) {

    if(mpitag == nullptr) {
        mpitag = new int[remoteHaloNumPartsCpu];
        for(int id = 0; id < remoteHaloNumPartsCpu; ++id)
            mpitag[id] = -1;
    }

    for(int id = 0; id < remoteHaloNumPartsCpu; ++id)
        postReceiveCpu(id,mpitag[id]);

}

template <class buf_t> void Tausch2D<buf_t>::packNextSendBufferCpu(size_t id, buf_t *buf) {

    if(numBuffersPackedCpu[id] == numBuffers)
        numBuffersPackedCpu[id] = 0;

    int size = localHaloSpecsCpu[id].width * localHaloSpecsCpu[id].height;
    for(int s = 0; s < size; ++s) {
        int index = (s/localHaloSpecsCpu[id].width + localHaloSpecsCpu[id].y)*localDim[TAUSCH_X] +
                    s%localHaloSpecsCpu[id].width + localHaloSpecsCpu[id].x;
        for(int val = 0; val < valuesPerPointPerBuffer[numBuffersPackedCpu[id]]; ++val) {
            int offset = 0;
            for(int b = 0; b < numBuffersPackedCpu[id]; ++b)
                offset += valuesPerPointPerBuffer[b]*size;
            mpiSendBuffer[id][offset + valuesPerPointPerBuffer[numBuffersPackedCpu[id]]*s + val] =
                    buf[valuesPerPointPerBuffer[numBuffersPackedCpu[id]]*index + val];
        }
    }
    ++numBuffersPackedCpu[id];

}

template <class buf_t> void Tausch2D<buf_t>::sendCpu(size_t id, int mpitag) {

    if(numBuffersPackedCpu[id] != numBuffers) {
        std::cerr << "[Tausch2D] ERROR: halo part " << id << " has " << numBuffersPackedCpu[id] << " out of "
                  << numBuffers << " send buffers packed... Abort!" << std::endl;
        exit(1);
    }

    if(!setupMpiSend[id]) {

        if(mpitag == -1) {
            std::cerr << "[Tausch2D] ERROR: MPI_Send for halo region #" << id << " hasn't been posted before, missing mpitag... Abort!" << std::endl;
            exit(1);
        }

        setupMpiSend[id] = true;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*localHaloSpecsCpu[id].width*localHaloSpecsCpu[id].height;
        MPI_Send_init(&mpiSendBuffer[id][0], bufsize, mpiDataType, localHaloSpecsCpu[id].remoteMpiRank,
                  mpitag, TAUSCH_COMM, &mpiSendRequests[id]);

    }

    MPI_Start(&mpiSendRequests[id]);

}

template <class buf_t> void Tausch2D<buf_t>::recvCpu(size_t id) {
    numBuffersUnpackedCpu[id] = 0;
    MPI_Wait(&mpiRecvRequests[id], MPI_STATUS_IGNORE);
}

template <class buf_t> void Tausch2D<buf_t>::unpackNextRecvBufferCpu(size_t id, buf_t *buf) {

    int size = remoteHaloSpecsCpu[id].width * remoteHaloSpecsCpu[id].height;
    for(int s = 0; s < size; ++s) {
        int index = (s/remoteHaloSpecsCpu[id].width + remoteHaloSpecsCpu[id].y)*localDim[TAUSCH_X] +
                    s%remoteHaloSpecsCpu[id].width + remoteHaloSpecsCpu[id].x;
        for(int val = 0; val < valuesPerPointPerBuffer[numBuffersUnpackedCpu[id]]; ++val) {
            int offset = 0;
            for(int b = 0; b < numBuffersUnpackedCpu[id]; ++b)
                offset += valuesPerPointPerBuffer[b]*size;
            buf[valuesPerPointPerBuffer[numBuffersUnpackedCpu[id]]*index + val] =
                    mpiRecvBuffer[id][offset + valuesPerPointPerBuffer[numBuffersUnpackedCpu[id]]*s + val];
        }
    }
    ++numBuffersUnpackedCpu[id];

}

template <class buf_t> void Tausch2D<buf_t>::packAndSendCpu(size_t id, buf_t *buf, int mpitag) {
    packNextSendBufferCpu(id, buf);
    sendCpu(id, mpitag);
}

template <class buf_t> void Tausch2D<buf_t>::recvAndUnpackCpu(size_t id, buf_t *buf) {
    recvCpu(id);
    unpackNextRecvBufferCpu(id, buf);
}


#ifdef TAUSCH_OPENCL

template <class buf_t> void Tausch2D<buf_t>::enableOpenCL(size_t *gpuDim, bool blockingSyncCpuGpu, int clLocalWorkgroupSize, bool giveOpenCLDeviceName) {

    this->blockingSyncCpuGpu = blockingSyncCpuGpu;
    cl_kernelLocalSize = clLocalWorkgroupSize;

    this->gpuDim[0] = gpuDim[0];
    this->gpuDim[1] = gpuDim[1];

    sync_counter[0].store(0); sync_counter[1].store(0);
    sync_lock[0].store(0); sync_lock[1].store(0);

    setupOpenCL(giveOpenCLDeviceName);

    try {
        cl_valuesPerPointPerBuffer = cl::Buffer(cl_context, &valuesPerPointPerBuffer[0], &valuesPerPointPerBuffer[numBuffers], true);
        cl_gpuDim = cl::Buffer(cl_context, &gpuDim[0], (&gpuDim[1])+1, true);
    } catch(cl::Error error) {
        std::cerr << "Tausch2D :: enableOpenCL() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch2D<buf_t>::setLocalHaloInfoCpuForGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumPartsCpuForGpu = numHaloParts;
    localHaloSpecsCpuForGpu = new TauschHaloSpec[numHaloParts];
    cpuToGpuSendBuffer = new std::atomic<buf_t>*[numHaloParts];
    numBuffersPackedCpuToGpu =  new size_t[numHaloParts]{};

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecsCpuForGpu[i].x = haloSpecs[i].x;
        localHaloSpecsCpuForGpu[i].y = haloSpecs[i].y;
        localHaloSpecsCpuForGpu[i].width = haloSpecs[i].width;
        localHaloSpecsCpuForGpu[i].height = haloSpecs[i].height;
        localHaloSpecsCpuForGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;
        localHaloSpecsCpuForGpu[i].gpu = haloSpecs[i].gpu;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        cpuToGpuSendBuffer[i] = new std::atomic<buf_t>[bufsize]{};

    }

}

template <class buf_t> void Tausch2D<buf_t>::setRemoteHaloInfoCpuForGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumPartsCpuForGpu = numHaloParts;
    remoteHaloSpecsCpuForGpu = new TauschHaloSpec[numHaloParts];
    gpuToCpuRecvBuffer = new buf_t*[numHaloParts];

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecsCpuForGpu[i].x = haloSpecs[i].x;
        remoteHaloSpecsCpuForGpu[i].y = haloSpecs[i].y;
        remoteHaloSpecsCpuForGpu[i].width = haloSpecs[i].width;
        remoteHaloSpecsCpuForGpu[i].height = haloSpecs[i].height;
        remoteHaloSpecsCpuForGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;
        remoteHaloSpecsCpuForGpu[i].gpu = haloSpecs[i].gpu;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        gpuToCpuRecvBuffer[i] = new buf_t[bufsize];

    }

}

template <class buf_t> void Tausch2D<buf_t>::setLocalHaloInfoGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    localHaloNumPartsGpu = numHaloParts;
    localHaloSpecsGpu = new TauschHaloSpec[numHaloParts];
    gpuToCpuSendBuffer = new std::atomic<buf_t>*[numHaloParts];

    try {
        cl_gpuToCpuSendBuffer = new cl::Buffer[numHaloParts];
        cl_localHaloSpecsGpu = new cl::Buffer[numHaloParts];
    } catch(cl::Error error) {
        std::cerr << "Tausch2D :: setLocalHaloInfo() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    for(int i = 0; i < numHaloParts; ++i) {

        localHaloSpecsGpu[i].x = haloSpecs[i].x;
        localHaloSpecsGpu[i].y = haloSpecs[i].y;
        localHaloSpecsGpu[i].width = haloSpecs[i].width;
        localHaloSpecsGpu[i].height = haloSpecs[i].height;
        localHaloSpecsGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;
        localHaloSpecsGpu[i].gpu = haloSpecs[i].gpu;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        gpuToCpuSendBuffer[i] = new std::atomic<buf_t>[bufsize]{};

        size_t tmpHaloSpecs[4] = {haloSpecs[i].x, haloSpecs[i].y, haloSpecs[i].width, haloSpecs[i].height};

        try {
            cl_gpuToCpuSendBuffer[i] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, bufsize*sizeof(double));
            cl_localHaloSpecsGpu[i] = cl::Buffer(cl_context, &tmpHaloSpecs[0], (&tmpHaloSpecs[3])+1, true);
        } catch(cl::Error error) {
            std::cerr << "Tausch2D :: setLocalHaloInfo() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
            exit(1);
        }

    }

}

template <class buf_t> void Tausch2D<buf_t>::setRemoteHaloInfoGpu(size_t numHaloParts, TauschHaloSpec *haloSpecs) {

    remoteHaloNumPartsGpu = numHaloParts;
    remoteHaloSpecsGpu = new TauschHaloSpec[numHaloParts];
    cpuToGpuRecvBuffer = new buf_t*[numHaloParts];

    try {
        cl_cpuToGpuRecvBuffer = new cl::Buffer[remoteHaloNumPartsGpu];
        cl_numBuffersUnpackedCpuToGpu = new cl::Buffer[remoteHaloNumPartsGpu];
        for(int i = 0; i < remoteHaloNumPartsGpu; ++i)
            cl_numBuffersUnpackedCpuToGpu[i] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, sizeof(double));
        cl_remoteHaloSpecsGpu = new cl::Buffer[numHaloParts];
    } catch(cl::Error error) {
        std::cerr << "Tausch2D :: enableOpenCL() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

    for(int i = 0; i < numHaloParts; ++i) {

        remoteHaloSpecsGpu[i].x = haloSpecs[i].x;
        remoteHaloSpecsGpu[i].y = haloSpecs[i].y;
        remoteHaloSpecsGpu[i].width = haloSpecs[i].width;
        remoteHaloSpecsGpu[i].height = haloSpecs[i].height;
        remoteHaloSpecsGpu[i].remoteMpiRank = haloSpecs[i].remoteMpiRank;
        remoteHaloSpecsGpu[i].gpu = haloSpecs[i].gpu;

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*haloSpecs[i].width*haloSpecs[i].height;
        cpuToGpuRecvBuffer[i] = new buf_t[bufsize];

        size_t tmpHaloSpecs[4] = {haloSpecs[i].x, haloSpecs[i].y, haloSpecs[i].width, haloSpecs[i].height};

        try {
            cl_cpuToGpuRecvBuffer[i] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, bufsize*sizeof(double));
            cl_remoteHaloSpecsGpu[i] = cl::Buffer(cl_context, &tmpHaloSpecs[0], (&tmpHaloSpecs[3])+1, true);
        } catch(cl::Error error) {
            std::cerr << "Tausch2D :: setRemoteHaloInfo() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
            exit(1);
        }

    }

}

template <class buf_t> void Tausch2D<buf_t>::packNextSendBufferCpuToGpu(size_t id, buf_t *buf) {

    if(!localHaloSpecsCpuForGpu[id].gpu)
        std::cout << "Tausch2D :: Warning: Using non-GPU halo for GPU halo exchange..." << std::endl;

    int size = localHaloSpecsCpuForGpu[id].width * localHaloSpecsCpuForGpu[id].height;
    for(int s = 0; s < size; ++s) {
        int index = (s/localHaloSpecsCpuForGpu[id].width + localHaloSpecsCpuForGpu[id].y)*localDim[TAUSCH_X] +
                    s%localHaloSpecsCpuForGpu[id].width +localHaloSpecsCpuForGpu[id].x;
        for(int val = 0; val < valuesPerPointPerBuffer[numBuffersPackedCpuToGpu[id]]; ++val) {
            int offset = 0;
            for(int b = 0; b < numBuffersPackedCpuToGpu[id]; ++b)
                offset += valuesPerPointPerBuffer[b]*size;
            cpuToGpuSendBuffer[id][offset + valuesPerPointPerBuffer[numBuffersPackedCpuToGpu[id]]*s + val].store(buf[valuesPerPointPerBuffer[numBuffersPackedCpuToGpu[id]]*index + val]);
        }
    }
    ++numBuffersPackedCpuToGpu[id];

}

template <class buf_t> void Tausch2D<buf_t>::sendCpuToGpu(size_t id) {
    numBuffersPackedCpuToGpu[id] = 0;
    syncCpuAndGpu();
}

template <class buf_t> void Tausch2D<buf_t>::recvCpuToGpu(size_t id) {

    syncCpuAndGpu();

    size_t bufsize = 0;
    for(int n = 0; n < numBuffers; ++n)
        bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsGpu[id].width*remoteHaloSpecsGpu[id].height;

    for(int j = 0; j < bufsize; ++j)
        cpuToGpuRecvBuffer[id][j] = cpuToGpuSendBuffer[id][j].load();

    try {
        cl_cpuToGpuRecvBuffer[id] = cl::Buffer(cl_context, &cpuToGpuRecvBuffer[id][0], (&cpuToGpuRecvBuffer[id][bufsize-1])+1, false);
        cl_queue.enqueueFillBuffer(cl_numBuffersUnpackedCpuToGpu[id], 0, 0, sizeof(double));
    } catch(cl::Error error) {
        std::cerr << "Tausch2D :: recvCpuToGpu() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch2D<buf_t>::unpackNextRecvBufferCpuToGpu(size_t id, cl::Buffer buf) {

    try {
        auto kernel_unpackRecvBuffer = cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer>
                                                (cl_programs, "unpackNextRecvBuffer");

        size_t bufsize = 0;
        for(int n = 0; n < numBuffers; ++n)
            bufsize += valuesPerPointPerBuffer[n]*remoteHaloSpecsGpu[id].width*remoteHaloSpecsGpu[id].height;

        int globalsize = (bufsize/cl_kernelLocalSize +1)*cl_kernelLocalSize;

        kernel_unpackRecvBuffer(cl::EnqueueArgs(cl_queue, cl::NDRange(globalsize), cl::NDRange(cl_kernelLocalSize)),
                                cl_gpuDim, cl_remoteHaloSpecsGpu[id], cl_valuesPerPointPerBuffer,
                                cl_numBuffersUnpackedCpuToGpu[id], cl_cpuToGpuRecvBuffer[id], buf);

        auto kernel_inc = cl::make_kernel<cl::Buffer>(cl_programs, "incrementBuffer");
        kernel_inc(cl::EnqueueArgs(cl_queue, cl::NDRange(1), cl::NDRange(1)), cl_numBuffersUnpackedCpuToGpu[id]);

    } catch(cl::Error error) {
        std::cerr << "Tausch2D :: unpackNextRecvBufferCpuToGpu() :: OpenCL exception caught: " << error.what()
                  << " (" << error.err() << ")" << std::endl;
        exit(1);
    }

}

template <class buf_t> void Tausch2D<buf_t>::syncCpuAndGpu() {

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

template <class buf_t> void Tausch2D<buf_t>::setupOpenCL(bool giveOpenCLDeviceName) {

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

template <class buf_t> void Tausch2D<buf_t>::compileKernels() {

    std::string oclstr = "";
    std::ifstream file;
    file.open("../../kernel.cl", std::ios::in);
    if(!file.is_open())
        std::cout << "error opening kernel file..." << std::endl;
    std::string str;
    while (std::getline(file, str)) {
        oclstr += str + "\n";
    }
    file.close();

    try {
        cl_programs = cl::Program(cl_context, oclstr, false);
        cl_programs.build();
    } catch(cl::Error error) {
        std::cout << "Tausch2D :: compileKernels() :: OpenCL exception caught: " << error.what() << " (" << error.err() << ")" << std::endl;
        if(error.err() == -11) {
            try {
                std::string log = cl_programs.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl_defaultDevice);
                std::cout << std::endl << " ******************** " << std::endl << " ** BUILD LOG" << std::endl
                          << " ******************** " << std::endl << log << std::endl << std::endl << " ******************** "
                          << std::endl << std::endl;
            } catch(cl::Error err) {
                std::cout << "Tausch2D :: compileKernels() :: getBuildInfo :: OpenCL exception caught: " << err.what() << " (" << err.err() << ")" << std::endl;
            }
        }
    }


}


#endif

template class Tausch2D<char>;
template class Tausch2D<char16_t>;
template class Tausch2D<char32_t>;
template class Tausch2D<wchar_t>;
template class Tausch2D<signed char>;
template class Tausch2D<short int>;
template class Tausch2D<int>;
template class Tausch2D<long>;
template class Tausch2D<long long>;
template class Tausch2D<unsigned char>;
template class Tausch2D<unsigned short int>;
template class Tausch2D<unsigned int>;
template class Tausch2D<unsigned long>;
template class Tausch2D<unsigned long long>;
template class Tausch2D<float>;
template class Tausch2D<double>;
template class Tausch2D<long double>;
template class Tausch2D<bool>;
