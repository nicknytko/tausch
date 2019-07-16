#ifndef TAUSCH_H
#define TAUSCH_H

#include <mpi.h>
#include <vector>
#include <array>
#include <map>
#include <cstring>
#include <memory>

#ifdef TAUSCH_OPENCL
#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#endif

#ifdef TAUSCH_CUDA
#include <cuda_runtime.h>
#endif

enum TauschOptimizationHint {
    NoHints = 1,
    UseMpiDerivedDatatype = 2,
    StaysOnDevice = 4,
    DoesNotStayOnDevice = 8,
};

template <class buf_t>
class Tausch {

public:
#ifdef TAUSCH_OPENCL
    Tausch(cl::Device device, cl::Context context, cl::CommandQueue queue, std::string cName4BufT,
           const MPI_Datatype mpiDataType, const MPI_Comm comm = MPI_COMM_WORLD, const bool useDuplicateOfCommunicator = true) {
#else
    Tausch(const MPI_Datatype mpiDataType, const MPI_Comm comm = MPI_COMM_WORLD, const bool useDuplicateOfCommunicator = true) {
#endif

        if(useDuplicateOfCommunicator)
            MPI_Comm_dup(comm, &TAUSCH_COMM);
        else
            TAUSCH_COMM = comm;

        this->mpiDataType = mpiDataType;

#ifdef TAUSCH_OPENCL
        this->device = device;
        this->context = context;
        this->queue = queue;
        this->cName4BufT = cName4BufT;

        clKernelLocalSize = 256;

        std::string oclstr = "typedef "+cName4BufT+" buf_t;";

        oclstr += R"d(

kernel void packSubRegion(global const buf_t * restrict inBuf, global buf_t * restrict outBuf,
                          global const int * restrict inIndices, const int numIndices,
                          const int bufferOffset) {

    int gid = get_global_id(0);

    if(gid < numIndices)
        outBuf[gid] = inBuf[bufferOffset + inIndices[gid]];

}

kernel void unpackSubRegion(global const buf_t * restrict inBuf, global buf_t * restrict outBuf,
                            global const int * restrict outIndices, const int numIndices,
                            const int bufferOffset) {

    int gid = get_global_id(0);

    if(gid < numIndices)
      outBuf[bufferOffset + outIndices[gid]] = inBuf[gid];

}
                             )d";

        try {

            programs = cl::Program(context, oclstr, false);
            programs.build("");

        } catch(cl::Error &e) {

            std::cout << "Tausch::Tausch(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;

            if(e.err() == -11) {
                try {
                    std::string log = programs.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
                    std::cout << std::endl << " ******************** " << std::endl << " ** BUILD LOG" << std::endl
                              << " ******************** " << std::endl << log << std::endl << std::endl << " ******************** "
                              << std::endl << std::endl;
                } catch(cl::Error &e) {
                    std::cout << "Tausch::Tausch()::getBuildInfo(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")"
                              << std::endl;
                }
            }

        }
#endif

    }

    inline int addLocalHaloInfo(std::vector<int> haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfo(extractHaloIndicesWithStride(haloIndices), numBuffers, remoteMpiRank, hints);
    }

    inline int addLocalHaloInfo(std::vector<size_t> haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfo(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())),
                                numBuffers, remoteMpiRank, hints);
    }

    inline int addLocalHaloInfo(std::vector<std::array<int, 4> > haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        localHaloIndices.push_back(haloIndices);
        localHaloIndicesSize.push_back(haloSize);
        localHaloNumBuffers.push_back(numBuffers);
        localHaloRemoteMpiRank.push_back(remoteMpiRank);

        localOptHints.push_back(hints);

        if(hints & UseMpiDerivedDatatype) {

            std::vector<MPI_Datatype> vectorDataTypes;
            std::vector<MPI_Aint> displacement;
            std::vector<int> blocklength;

            vectorDataTypes.reserve(haloIndices.size());
            displacement.reserve(haloIndices.size());
            blocklength.reserve(haloIndices.size());

            for(auto const & item : haloIndices) {

                MPI_Datatype vec;
                MPI_Type_vector(item[2], item[1], item[3], mpiDataType, &vec);
                MPI_Type_commit(&vec);

                vectorDataTypes.push_back(vec);
                displacement.push_back(item[0]*sizeof(buf_t));
                blocklength.push_back(1);

            }

            MPI_Datatype newtype;
            MPI_Type_create_struct(haloIndices.size(), blocklength.data(), displacement.data(), vectorDataTypes.data(), &newtype);
            MPI_Type_commit(&newtype);

            sendDatatype.push_back(newtype);

            sendBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));


        } else {

            void *newbuf = NULL;
            posix_memalign(&newbuf, 64, numBuffers*haloSize*sizeof(buf_t));
            buf_t *newbuf_buft = reinterpret_cast<buf_t*>(newbuf);
            double zero = 0;
            std::fill_n(newbuf_buft, numBuffers*haloSize, zero);
            sendBuffer.push_back(std::unique_ptr<buf_t[]>(std::move(newbuf_buft)));

        }

        mpiSendRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));

        setupMpiSend.push_back(false);

        return sendBuffer.size()-1;

    }


    inline int addRemoteHaloInfo(std::vector<int> haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfo(extractHaloIndicesWithStride(haloIndices), numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfo(std::vector<size_t> haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfo(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())),
                                 numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfo(std::vector<std::array<int, 4> > haloIndices, const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto const & tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        remoteHaloIndices.push_back(haloIndices);
        remoteHaloIndicesSize.push_back(haloSize);
        remoteHaloNumBuffers.push_back(numBuffers);
        remoteHaloRemoteMpiRank.push_back(remoteMpiRank);

        remoteOptHints.push_back(hints);

        if(hints & UseMpiDerivedDatatype) {

            std::vector<MPI_Datatype> vectorDataTypes;
            std::vector<MPI_Aint> displacement;
            std::vector<int> blocklength;

            vectorDataTypes.reserve(haloIndices.size());
            displacement.reserve(haloIndices.size());
            blocklength.reserve(haloIndices.size());

            for(auto const & item : haloIndices) {

                MPI_Datatype vec;
                MPI_Type_vector(item[2], item[1], item[3], mpiDataType, &vec);
                MPI_Type_commit(&vec);

                vectorDataTypes.push_back(vec);
                displacement.push_back(item[0]*sizeof(buf_t));
                blocklength.push_back(1);

            }

            MPI_Datatype newtype;
            MPI_Type_create_struct(haloIndices.size(), blocklength.data(), displacement.data(), vectorDataTypes.data(), &newtype);
            MPI_Type_commit(&newtype);

            recvDatatype.push_back(newtype);

            recvBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));

        } else {

            void *newbuf = NULL;
            posix_memalign(&newbuf, 64, numBuffers*haloSize*sizeof(buf_t));
            buf_t *newbuf_buft = reinterpret_cast<buf_t*>(newbuf);
            double zero = 0;
            std::fill_n(newbuf_buft, numBuffers*haloSize, zero);
            recvBuffer.push_back(std::unique_ptr<buf_t[]>(std::move(newbuf_buft)));

        }

        mpiRecvRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));

        setupMpiRecv.push_back(false);

        return recvBuffer.size()-1;

    }

    inline void packSendBuffer(const size_t haloId, const size_t bufferId, const buf_t *buf) const {

        const size_t haloSize = localHaloIndicesSize[haloId];

        size_t mpiSendBufferIndex = 0;
        for(auto const & region : localHaloIndices[haloId]) {

            const int &region_start = region[0];
            const int &region_howmanycols = region[1];
            const int &region_howmanyrows = region[2];
            const int &region_stridecol = region[3];

            if(region_howmanycols == 1) {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {
                    sendBuffer[haloId][bufferId*haloSize + mpiSendBufferIndex] = buf[region_start + rows*region_stridecol];
                    ++mpiSendBufferIndex;
                }

            } else if(region_howmanycols == 2) {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {
                    sendBuffer[haloId][bufferId*haloSize + mpiSendBufferIndex  ] = buf[region_start + rows*region_stridecol   ];
                    sendBuffer[haloId][bufferId*haloSize + mpiSendBufferIndex+1] = buf[region_start + rows*region_stridecol +1];
                    mpiSendBufferIndex += 2;
                }

            } else {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {

                    memcpy(&sendBuffer[haloId][bufferId*haloSize + mpiSendBufferIndex], &buf[region_start + rows*region_stridecol], region_howmanycols*sizeof(buf_t));
                    mpiSendBufferIndex += region_howmanycols;

                }

            }

        }

    }

    inline void packSendBuffer(const size_t haloId, const size_t bufferId, const buf_t *buf,
                               std::vector<size_t> overwriteHaloSendIndices, std::vector<size_t> overwriteHaloSourceIndices) const {

        size_t haloSize = localHaloIndicesSize[haloId];

        for(auto index = 0; index < overwriteHaloSendIndices.size(); ++index)
            sendBuffer[haloId][bufferId*haloSize + overwriteHaloSendIndices[index]] = buf[overwriteHaloSourceIndices[index]];

    }

    inline MPI_Request *send(size_t haloId, const int msgtag, int remoteMpiRank = -1, const buf_t *buf = nullptr, const bool blocking = false, MPI_Comm overwriteComm = MPI_COMM_NULL) {

        if(localHaloIndices[haloId].size() == 0)
            return nullptr;

        if(overwriteComm == MPI_COMM_NULL)
            overwriteComm = TAUSCH_COMM;

        if(localOptHints[haloId] & UseMpiDerivedDatatype) {

            if(remoteMpiRank == -1)
                remoteMpiRank = localHaloRemoteMpiRank[haloId];

            MPI_Isend(buf, 1, sendDatatype[haloId], remoteMpiRank, msgtag, overwriteComm, mpiSendRequests[haloId].get());
            if(blocking)
                MPI_Wait(mpiSendRequests[haloId].get(), MPI_STATUS_IGNORE);

            return mpiSendRequests[haloId].get();

        } else {

            if(!setupMpiSend[haloId]) {

                setupMpiSend[haloId] = true;

                // if we stay on the same rank, we don't need to use MPI
                int myRank;
                MPI_Comm_rank(overwriteComm, &myRank);
                if(remoteMpiRank == myRank) {
                    msgtagToHaloId[myRank*1000000 + msgtag] = haloId;
                    return nullptr;
                }
                MPI_Send_init(sendBuffer[haloId].get(), localHaloNumBuffers[haloId]*localHaloIndicesSize[haloId], mpiDataType, remoteMpiRank,
                          msgtag, overwriteComm, mpiSendRequests[haloId].get());

            } else
                MPI_Wait(mpiSendRequests[haloId].get(), MPI_STATUS_IGNORE);

            MPI_Start(mpiSendRequests[haloId].get());
            if(blocking)
                MPI_Wait(mpiSendRequests[haloId].get(), MPI_STATUS_IGNORE);

            return mpiSendRequests[haloId].get();

        }

    }

    inline MPI_Request *recv(size_t haloId, const int msgtag, int remoteMpiRank = -1, buf_t *buf = nullptr, const bool blocking = true, MPI_Comm overwriteComm = MPI_COMM_NULL) {

        if(remoteHaloIndices[haloId].size() == 0)
            return nullptr;

        if(overwriteComm == MPI_COMM_NULL)
            overwriteComm = TAUSCH_COMM;

        if(remoteOptHints[haloId] & UseMpiDerivedDatatype) {

            if(remoteMpiRank == -1)
                remoteMpiRank = remoteHaloRemoteMpiRank[haloId];

            MPI_Irecv(buf, 1, recvDatatype[haloId], remoteMpiRank, msgtag, overwriteComm, mpiRecvRequests[haloId].get());
            if(blocking)
                MPI_Wait(mpiRecvRequests[haloId].get(), MPI_STATUS_IGNORE);

            return mpiRecvRequests[haloId].get();

        } else {

            if(!setupMpiRecv[haloId]) {

                if(remoteMpiRank == -1)
                    remoteMpiRank = remoteHaloRemoteMpiRank[haloId];

                // if we stay on the same rank, we don't need to use MPI
                int myRank;
                MPI_Comm_rank(overwriteComm, &myRank);

                if(remoteMpiRank == myRank) {

                    const int remoteHaloId = msgtagToHaloId[myRank*1000000 + msgtag];

                    memcpy(recvBuffer[haloId].get(), sendBuffer[remoteHaloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId]*sizeof(buf_t));

                } else {

                    setupMpiRecv[haloId] = true;

                    MPI_Recv_init(recvBuffer[haloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId], mpiDataType,
                                  remoteMpiRank, msgtag, overwriteComm, mpiRecvRequests[haloId].get());
                }

            }

            // this will remain false if we remained on the same mpi rank
            if(setupMpiRecv[haloId]) {

                MPI_Start(mpiRecvRequests[haloId].get());
                if(blocking)
                    MPI_Wait(mpiRecvRequests[haloId].get(), MPI_STATUS_IGNORE);

                return mpiRecvRequests[haloId].get();

            } else

                return nullptr;

        }

    }

    inline void unpackRecvBuffer(const size_t haloId, const size_t bufferId, buf_t *buf) const {

        size_t haloSize = remoteHaloIndicesSize[haloId];

        size_t mpiRecvBufferIndex = 0;

        for(auto const & region : remoteHaloIndices[haloId]) {


            const auto &region_start = region[0];
            const auto &region_howmanycols = region[1];
            const auto &region_howmanyrows = region[2];
            const auto &region_stridecol = region[3];

            if(region_howmanycols == 1) {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {
                    buf[region_start + rows*region_stridecol] = recvBuffer[haloId][bufferId*haloSize + mpiRecvBufferIndex];
                    ++mpiRecvBufferIndex;
                }

            } else if(region_howmanycols == 2) {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {
                    buf[region_start + rows*region_stridecol   ] = recvBuffer[haloId][bufferId*haloSize + mpiRecvBufferIndex   ];
                    buf[region_start + rows*region_stridecol +1] = recvBuffer[haloId][bufferId*haloSize + mpiRecvBufferIndex +1];
                    mpiRecvBufferIndex += 2;
                }

            } else {

                for(int rows = 0; rows < region_howmanyrows; ++rows) {

                    memcpy(&buf[region_start + rows*region_stridecol], &recvBuffer[haloId][bufferId*haloSize + mpiRecvBufferIndex], region_howmanycols*sizeof(buf_t));
                    mpiRecvBufferIndex += region_howmanycols;

                }

            }

        }

    }

    inline void unpackRecvBuffer(const size_t haloId, const size_t bufferId, buf_t *buf,
                                 std::vector<size_t> overwriteHaloRecvIndices, std::vector<size_t> overwriteHaloTargetIndices) const {

        size_t haloSize = remoteHaloIndicesSize[haloId];

        for(size_t index = 0; index < overwriteHaloRecvIndices.size(); ++index)
            buf[overwriteHaloTargetIndices[index]] = recvBuffer[haloId][bufferId*haloSize + overwriteHaloRecvIndices[index]];

    }

    inline MPI_Request *packAndSend(const size_t haloId, const buf_t *buf, const int msgtag, const int remoteMpiRank = -1) const {
        packSendBuffer(haloId, 0, buf);
        return send(haloId, msgtag, remoteMpiRank);
    }

    inline void recvAndUnpack(const size_t haloId, buf_t *buf, const int msgtag, const int remoteMpiRank = -1) const {
        recv(haloId, msgtag, remoteMpiRank, true);
        unpackRecvBuffer(haloId, 0, buf);
    }


#ifdef TAUSCH_OPENCL

    inline int addLocalHaloInfoOCL(std::vector<int> haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfoOCL(extractHaloIndicesWithStride(haloIndices),
                                   numBuffers, remoteMpiRank, hints);
    }

    inline int addLocalHaloInfoOCL(std::vector<size_t> haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfoOCL(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())),
                                   numBuffers, remoteMpiRank, hints);
    }

    inline int addLocalHaloInfoOCL(std::vector<std::array<int, 4> > haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        try {

            if(haloSize == 0) {

                localHaloIndices.push_back({});
                localHaloIndicesSize.push_back(haloIndices.size());
                localHaloNumBuffers.push_back(numBuffers);
                localHaloRemoteMpiRank.push_back(remoteMpiRank);

                localOptHints.push_back(hints);

                sendBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));
                mpiSendRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
                setupMpiSend.push_back(false);

            } else {

                localHaloIndices.push_back(haloIndices);
                localHaloIndicesSize.push_back(haloSize);
                localHaloNumBuffers.push_back(numBuffers);
                localHaloRemoteMpiRank.push_back(remoteMpiRank);

                localOptHints.push_back(hints);

                sendBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[numBuffers*haloSize]));
                mpiSendRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
                setupMpiSend.push_back(false);

            }

        } catch(cl::Error &e) {
            std::cout << "Tausch::addLocalHaloInfoOCL(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }

        return sendBuffer.size()-1;

    }

    inline int addRemoteHaloInfoOCL(std::vector<int> haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfoOCL(extractHaloIndicesWithStride(haloIndices),
                                    numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfoOCL(std::vector<size_t> haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfoOCL(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())),
                                    numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfoOCL(std::vector<std::array<int, 4> > haloIndices, const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        try {

            if(haloSize == 0) {

                remoteHaloIndices.push_back({});
                remoteHaloIndicesSize.push_back(0);
                remoteHaloNumBuffers.push_back(numBuffers);
                remoteHaloRemoteMpiRank.push_back(remoteMpiRank);

                remoteOptHints.push_back(hints);

                recvBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));
                mpiRecvRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
                setupMpiRecv.push_back(false);

            } else {

                remoteHaloIndices.push_back(haloIndices);
                remoteHaloIndicesSize.push_back(haloSize);
                remoteHaloNumBuffers.push_back(numBuffers);
                remoteHaloRemoteMpiRank.push_back(remoteMpiRank);

                remoteOptHints.push_back(hints);

                recvBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[numBuffers*haloSize]));
                mpiRecvRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
                setupMpiRecv.push_back(false);

            }

        } catch(cl::Error &e) {
            std::cout << "Tausch::addRemoteHaloInfoOCL(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }

        return recvBuffer.size()-1;

    }

    void packSendBufferOCL(const int haloId, int bufferId, cl::Buffer buf) {

        try {

            const size_t haloSize = localHaloIndicesSize[haloId];

            size_t mpiSendBufferIndex = 0;
            for(size_t iRegion = 0; iRegion < localHaloIndices[haloId].size(); ++iRegion) {
                const std::array<int, 4> vals = localHaloIndices[haloId][iRegion];

                const int val_start = vals[0];
                const int val_howmanycols = vals[1];
                const int val_howmanyrows = vals[2];
                const int val_striderows = vals[3];

                for(int rows = 0; rows < val_howmanyrows; ++rows) {

                    cl::size_t<3> buffer_offset;
                    buffer_offset[0] = (val_start+rows*val_striderows)*sizeof(buf_t); buffer_offset[1] = 0; buffer_offset[2] = 0;
                    cl::size_t<3> host_offset;
                    host_offset[0] = (bufferId*haloSize + mpiSendBufferIndex)*sizeof(buf_t); host_offset[1] = 0; host_offset[2] = 0;

                    cl::size_t<3> region;
                    region[0] = sizeof(buf_t); region[1] = val_howmanycols; region[2] = 1;

                    queue.enqueueReadBufferRect(buf,
                                                CL_TRUE,
                                                buffer_offset,
                                                host_offset,
                                                region,
                                                sizeof(buf_t),
                                                0,
                                                sizeof(buf_t),
                                                0,
                                                sendBuffer[haloId].get());

                    mpiSendBufferIndex += val_howmanycols;
                }

            }

        } catch(cl::Error &e) {
            std::cerr << "Tausch::packSendBufferOCL(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }

    }

    void packSendBufferOCL(const int haloId, int bufferId, cl::Buffer buf,
                           const std::vector<int> overwriteHaloSendIndices, const std::vector<int> overwriteHaloSourceIndices) {

        const size_t haloSize = localHaloIndicesSize[haloId];

        try {
            auto kernel_pack = cl::make_kernel
                                    <const cl::Buffer &, cl::Buffer &, const cl::Buffer &, const int &, const int &>
                                    (programs, "packSubRegion");

            cl::Buffer clHaloIndicesIn(context, overwriteHaloSourceIndices.begin(), overwriteHaloSourceIndices.end(), true);

            int globalsize = (overwriteHaloSourceIndices.size()/clKernelLocalSize +1)*clKernelLocalSize;

            cl::Buffer tmpSendBuffer_d(context, CL_MEM_READ_WRITE, overwriteHaloSourceIndices.size()*sizeof(buf_t));

            kernel_pack(cl::EnqueueArgs(queue, cl::NDRange(globalsize), cl::NDRange(clKernelLocalSize)),
                          buf, tmpSendBuffer_d, clHaloIndicesIn, overwriteHaloSourceIndices.size(), bufferId*haloSize);

            std::unique_ptr<buf_t[]> tmpSendBuffer_h(new buf_t[overwriteHaloSourceIndices.size()]);
            cl::copy(queue, tmpSendBuffer_d, tmpSendBuffer_h.get(), &tmpSendBuffer_h[overwriteHaloSourceIndices.size()]);

            for(size_t i = 0; i < overwriteHaloSendIndices.size(); ++i)
                sendBuffer[haloId][bufferId*haloSize + overwriteHaloSendIndices[i]] = tmpSendBuffer_h[i];

        } catch(cl::Error &e) {
            std::cerr << "Tausch::packSendBufferOCL(): OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }

    }

    MPI_Request *sendOCL(const int haloId, const int msgtag, int remoteMpiRank = -1) {

        if(localHaloIndicesSize[haloId] == 0)
            return nullptr;

        if(!setupMpiSend[haloId]) {

            if(remoteMpiRank == -1)
                remoteMpiRank = localHaloRemoteMpiRank[haloId];

            // if we stay on the same rank, we don't need to use MPI
            int myRank;
            MPI_Comm_rank(TAUSCH_COMM, &myRank);
            if(remoteMpiRank == myRank) {
                msgtagToHaloId[myRank*1000000 + msgtag] = haloId;
                return nullptr;
            }

            setupMpiSend[haloId] = true;

            MPI_Send_init(sendBuffer[haloId].get(), localHaloNumBuffers[haloId]*localHaloIndicesSize[haloId],
                          mpiDataType, remoteMpiRank, msgtag, TAUSCH_COMM, mpiSendRequests[haloId].get());

        } else
            MPI_Wait(mpiSendRequests[haloId].get(), MPI_STATUS_IGNORE);

        MPI_Start(mpiSendRequests[haloId].get());

        return mpiSendRequests[haloId].get();

    }

    void recvOCL(const int haloId, const int msgtag, int remoteMpiRank = -1) {

        if(remoteHaloIndicesSize[haloId] == 0)
            return;

        if(!setupMpiRecv[haloId]) {

            if(remoteMpiRank == -1)
                remoteMpiRank = remoteHaloRemoteMpiRank[haloId];

            // if we stay on the same rank, we don't need to use MPI
            int myRank;
            MPI_Comm_rank(TAUSCH_COMM, &myRank);

            if(remoteMpiRank == myRank) {

                const int remoteHaloId = msgtagToHaloId[myRank*1000000 + msgtag];

                memcpy(recvBuffer[haloId].get(), sendBuffer[remoteHaloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId]*sizeof(buf_t));

            } else {

                setupMpiRecv[haloId] = true;

                MPI_Recv_init(recvBuffer[haloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId], mpiDataType,
                              remoteMpiRank, msgtag, TAUSCH_COMM, mpiRecvRequests[haloId].get());

            }

        }

        // this will remain false if we remained on the same mpi rank
        if(setupMpiRecv[haloId]) {

            MPI_Start(mpiRecvRequests[haloId].get());
            MPI_Wait(mpiRecvRequests[haloId].get(), MPI_STATUS_IGNORE);

        }

    }

    void unpackRecvBufferOCL(const int haloId, const int bufferId, cl::Buffer buf) {

        try {

            const size_t haloSize = remoteHaloIndicesSize[haloId];

            size_t mpiRecvBufferIndex = 0;
            for(size_t iRegion = 0; iRegion < remoteHaloIndices[haloId].size(); ++iRegion) {
                const std::array<int, 4> vals = remoteHaloIndices[haloId][iRegion];

                const int val_start = vals[0];
                const int val_howmanycols = vals[1];
                const int val_howmanyrows = vals[2];
                const int val_striderows = vals[3];

                for(int rows = 0; rows < val_howmanyrows; ++rows) {

                    cl::size_t<3> buffer_offset;
                    buffer_offset[0] = (val_start+rows*val_striderows)*sizeof(buf_t); buffer_offset[1] = 0; buffer_offset[2] = 0;
                    cl::size_t<3> host_offset;
                    host_offset[0] = (bufferId*haloSize + mpiRecvBufferIndex)*sizeof(buf_t); host_offset[1] = 0; host_offset[2] = 0;

                    cl::size_t<3> region;
                    region[0] = sizeof(buf_t); region[1] = val_howmanycols; region[2] = 1;

                    queue.enqueueWriteBufferRect(buf,
                                                 CL_TRUE,
                                                 buffer_offset,
                                                 host_offset,
                                                 region,
                                                 sizeof(buf_t),
                                                 0,
                                                 sizeof(buf_t),
                                                 0,
                                                 recvBuffer[haloId].get());

                    mpiRecvBufferIndex += val_howmanycols;

                }

            }

        } catch(cl::Error &e) {
            std::cerr << "Tausch::unpackRecvBufferOCL() :: OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }

    }

    void unpackRecvBufferOCL(const int haloId, int bufferId, cl::Buffer buf,
                          const std::vector<int> overwriteHaloRecvIndices, const std::vector<int> overwriteHaloTargetIndices) {

        const size_t haloSize = localHaloIndicesSize[haloId];

        try {

            auto kernel_unpack = cl::make_kernel
                                    <const cl::Buffer &, cl::Buffer &, const cl::Buffer &, const int &, const int &>
                                    (programs, "unpackSubRegion");

            cl::Buffer clHaloIndicesOut(context, overwriteHaloTargetIndices.begin(), overwriteHaloTargetIndices.end(), true);

            std::unique_ptr<buf_t[]> tmpRecvBuffer_h(new buf_t[overwriteHaloTargetIndices.size()]);
            for(size_t i = 0; i < overwriteHaloTargetIndices.size(); ++i)
                tmpRecvBuffer_h[i] = recvBuffer[haloId][bufferId*haloSize + overwriteHaloRecvIndices[i]];

            cl::Buffer tmpRecvBuffer_d(context, CL_MEM_READ_WRITE, overwriteHaloRecvIndices.size()*sizeof(buf_t));
            cl::copy(queue, tmpRecvBuffer_h.get(), &tmpRecvBuffer_h[overwriteHaloRecvIndices.size()], tmpRecvBuffer_d);

            int globalsize = (overwriteHaloRecvIndices.size()/clKernelLocalSize +1)*clKernelLocalSize;

            kernel_unpack(cl::EnqueueArgs(queue, cl::NDRange(globalsize), cl::NDRange(clKernelLocalSize)),
                          tmpRecvBuffer_d, buf, clHaloIndicesOut, overwriteHaloTargetIndices.size(), bufferId*haloSize);

        } catch(cl::Error &e) {
            std::cerr << "Tausch::unpackRecvBufferOCL() :: OpenCL exception caught: " << e.what() << " (" << e.err() << ")" << std::endl;
        }
    }

    inline MPI_Request *packAndSendOCL(const int haloId, const cl::Buffer buf, const int msgtag, const int remoteMpiRank = -1) const {
        packSendBufferOCL(haloId, 0, buf);
        return sendOCL(haloId, msgtag, remoteMpiRank);
    }

    inline void recvAndUnpackOCL(const int haloId, cl::Buffer buf, const int msgtag, const int remoteMpiRank = -1) const {
        recvOCL(haloId, msgtag, remoteMpiRank);
        unpackRecvBuffer(haloId, 0, buf);
    }

#endif

#ifdef TAUSCH_CUDA

    inline int addLocalHaloInfoCUDA(std::vector<int> haloIndices,
                                    const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfoCUDA(extractHaloIndicesWithStride(haloIndices), numBuffers, remoteMpiRank, hints);
    }
    inline int addLocalHaloInfoCUDA(std::vector<size_t> haloIndices,
                                    const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addLocalHaloInfoCUDA(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())), remoteMpiRank, hints);
    }
    inline int addLocalHaloInfoCUDA(std::vector<std::array<int, 4> > haloIndices,
                                    const size_t numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        if(haloSize == 0) {

            localHaloIndices.push_back({});
            localHaloIndicesSize.push_back(0);
            localHaloNumBuffers.push_back(numBuffers);
            localHaloRemoteMpiRank.push_back(remoteMpiRank);

            localOptHints.push_back(hints);

            sendBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));
            mpiSendRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
            setupMpiSend.push_back(false);

        } else {

            localHaloIndices.push_back(haloIndices);
            localHaloIndicesSize.push_back(haloSize);
            localHaloNumBuffers.push_back(numBuffers);
            localHaloRemoteMpiRank.push_back(remoteMpiRank);

            localOptHints.push_back(hints);

            sendBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[numBuffers*haloSize]));
            mpiSendRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
            setupMpiSend.push_back(false);

        }

        return sendBuffer.size()-1;

    }

    inline int addRemoteHaloInfoCUDA(std::vector<int> haloIndices,
                                     const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfoCUDA(extractHaloIndicesWithStride(haloIndices), numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfoCUDA(std::vector<size_t> haloIndices,
                                     const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {
        return addRemoteHaloInfoCUDA(extractHaloIndicesWithStride(std::vector<int>(haloIndices.begin(), haloIndices.end())), numBuffers, remoteMpiRank, hints);
    }

    inline int addRemoteHaloInfoCUDA(std::vector<std::array<int, 4> > haloIndices,
                                     const int numBuffers = 1, const int remoteMpiRank = -1, TauschOptimizationHint hints = TauschOptimizationHint::NoHints) {

        int haloSize = 0;
        for(auto tuple : haloIndices)
            haloSize += tuple[1]*tuple[2];

        if(haloSize == 0) {

            remoteHaloIndices.push_back({});
            remoteHaloIndicesSize.push_back(0);
            remoteHaloNumBuffers.push_back(numBuffers);
            remoteHaloRemoteMpiRank.push_back(remoteMpiRank);

            remoteOptHints.push_back(hints);

            recvBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[1]));
            mpiRecvRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
            setupMpiRecv.push_back(false);

        } else {

            remoteHaloIndices.push_back(haloIndices);
            remoteHaloIndicesSize.push_back(haloSize);
            remoteHaloNumBuffers.push_back(numBuffers);
            remoteHaloRemoteMpiRank.push_back(remoteMpiRank);

            remoteOptHints.push_back(hints);

            recvBuffer.push_back(std::unique_ptr<buf_t[]>(new buf_t[numBuffers*haloSize]));
            mpiRecvRequests.push_back(std::unique_ptr<MPI_Request>(new MPI_Request));
            setupMpiRecv.push_back(false);

        }

        return recvBuffer.size()-1;

    }


    void packSendBufferCUDA(const int haloId, int bufferId, buf_t *buf_d) {

        const size_t haloSize = localHaloIndicesSize[haloId];

        if(localOptHints[haloId] == TauschOptimizationHint::StaysOnDevice) {

            if(sendCommunicationBufferKeptOnCuda.find(haloId) == sendCommunicationBufferKeptOnCuda.end()) {

                buf_t *cudabuf;
                cudaMalloc(&cudabuf, localHaloNumBuffers[haloId]*haloSize*sizeof(buf_t));
                sendCommunicationBufferKeptOnCuda[haloId] = cudabuf;

            }

            size_t mpiSendBufferIndex = 0;
            for(size_t region = 0; region < localHaloIndices[haloId].size(); ++region) {
                const std::array<int, 4> vals = localHaloIndices[haloId][region];

                const int val_start = vals[0];
                const int val_howmanycols = vals[1];
                const int val_howmanyrows = vals[2];
                const int val_striderows = vals[3];

                for(int rows = 0; rows < val_howmanyrows; ++rows) {

                    cudaError_t err = cudaMemcpy2D(&sendCommunicationBufferKeptOnCuda[haloId][bufferId*haloSize + mpiSendBufferIndex], sizeof(buf_t),
                                                   &buf_d[val_start+rows*val_striderows], sizeof(buf_t),
                                                   sizeof(buf_t), val_howmanycols, cudaMemcpyDeviceToDevice);
                    if(err != cudaSuccess)
                        std::cout << "Tausch::packSendBufferCUDA() 1: CUDA error detected: " << err << std::endl;

                    mpiSendBufferIndex += val_howmanycols;

                }

            }

            return;

        }

        size_t mpiSendBufferIndex = 0;
        for(size_t region = 0; region < localHaloIndices[haloId].size(); ++region) {
            const std::array<int, 4> vals = localHaloIndices[haloId][region];

            const int val_start = vals[0];
            const int val_howmanycols = vals[1];
            const int val_howmanyrows = vals[2];
            const int val_striderows = vals[3];

            for(int rows = 0; rows < val_howmanyrows; ++rows) {

                cudaError_t err = cudaMemcpy2D(&sendBuffer[haloId][bufferId*haloSize + mpiSendBufferIndex], sizeof(buf_t),
                                               &buf_d[val_start+rows*val_striderows], sizeof(buf_t),
                                               sizeof(buf_t), val_howmanycols, cudaMemcpyDeviceToHost);
                if(err != cudaSuccess)
                    std::cout << "Tausch::packSendBufferCUDA() 2: CUDA error detected: " << err << std::endl;

                mpiSendBufferIndex += val_howmanycols;

            }

        }

    }

    inline MPI_Request *sendCUDA(size_t haloId, const int msgtag, int remoteMpiRank = -1) {

        if(localHaloIndicesSize[haloId] == 0)
            return nullptr;

        if(!setupMpiSend[haloId]) {

            if(remoteMpiRank == -1)
                remoteMpiRank = localHaloRemoteMpiRank[haloId];

            // if we stay on the same rank, we don't need to use MPI
            int myRank;
            MPI_Comm_rank(TAUSCH_COMM, &myRank);
            if(remoteMpiRank == myRank) {
                msgtagToHaloId[myRank*1000000 + msgtag] = haloId;
                return nullptr;
            }

            setupMpiSend[haloId] = true;

            MPI_Send_init(sendBuffer[haloId].get(), localHaloNumBuffers[haloId]*localHaloIndicesSize[haloId], mpiDataType,
                          remoteMpiRank, msgtag, TAUSCH_COMM, mpiSendRequests[haloId].get());

        } else
            MPI_Wait(mpiSendRequests[haloId].get(), MPI_STATUS_IGNORE);

        MPI_Start(mpiSendRequests[haloId].get());

        return mpiSendRequests[haloId].get();

    }

    inline MPI_Request *recvCUDA(size_t haloId, const int msgtag, int remoteMpiRank = -1, const bool blocking = true) {

        if(remoteHaloIndicesSize[haloId] == 0)
            return nullptr;

        if(!setupMpiRecv[haloId]) {

            if(remoteMpiRank == -1)
                remoteMpiRank = remoteHaloRemoteMpiRank[haloId];

            int myRank;
            MPI_Comm_rank(TAUSCH_COMM, &myRank);

            const int remoteHaloId = msgtagToHaloId[myRank*1000000 + msgtag];

            if(remoteMpiRank == myRank && remoteOptHints[haloId] == TauschOptimizationHint::StaysOnDevice) {

                buf_t *cudabuf;
                cudaMalloc(&cudabuf, remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId]*sizeof(buf_t));
                cudaMemcpy(cudabuf, sendCommunicationBufferKeptOnCuda[remoteHaloId], remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId]*sizeof(buf_t), cudaMemcpyDeviceToDevice);
                recvCommunicationBufferKeptOnCuda[haloId] = cudabuf;

            } else if(remoteMpiRank == myRank) {

                memcpy(recvBuffer[haloId].get(), sendBuffer[remoteHaloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId]*sizeof(buf_t));

            } else {

                setupMpiRecv[haloId] = true;

                MPI_Recv_init(recvBuffer[haloId].get(), remoteHaloNumBuffers[haloId]*remoteHaloIndicesSize[haloId], mpiDataType,
                              remoteMpiRank, msgtag, TAUSCH_COMM, mpiRecvRequests[haloId].get());

            }

        }

        // this will remain false if we remained on the same mpi rank
        if(setupMpiRecv[haloId]) {

            MPI_Start(mpiRecvRequests[haloId].get());
            if(blocking)
                MPI_Wait(mpiRecvRequests[haloId].get(), MPI_STATUS_IGNORE);

            return mpiRecvRequests[haloId].get();

        } else
            return nullptr;

    }

    inline void unpackRecvBufferCUDA(const size_t haloId, const size_t bufferId, buf_t *buf_d) {

        size_t haloSize = remoteHaloIndicesSize[haloId];

        if(remoteOptHints[haloId] == TauschOptimizationHint::StaysOnDevice) {

            size_t mpiRecvBufferIndex = 0;
            for(size_t region = 0; region < remoteHaloIndices[haloId].size(); ++region) {
                const std::array<int, 4> vals = remoteHaloIndices[haloId][region];

                const int val_start = vals[0];
                const int val_howmanycols = vals[1];
                const int val_howmanyrows = vals[2];
                const int val_striderows = vals[3];

                for(int rows = 0; rows < val_howmanyrows; ++rows) {

                    cudaError_t err = cudaMemcpy2D(&buf_d[val_start+rows*val_striderows], sizeof(buf_t),
                                                   &recvCommunicationBufferKeptOnCuda[haloId][bufferId*haloSize + mpiRecvBufferIndex], sizeof(buf_t),
                                                   sizeof(buf_t), val_howmanycols, cudaMemcpyDeviceToDevice);
                    if(err != cudaSuccess)
                        std::cout << "Tausch::unpackRecvBufferCUDA(): CUDA error detected: " << err << std::endl;

                    mpiRecvBufferIndex += val_howmanycols;

                }

            }

        } else {

            size_t mpiRecvBufferIndex = 0;
            for(size_t region = 0; region < remoteHaloIndices[haloId].size(); ++region) {
                const std::array<int, 4> vals = remoteHaloIndices[haloId][region];

                const int val_start = vals[0];
                const int val_howmanycols = vals[1];
                const int val_howmanyrows = vals[2];
                const int val_striderows = vals[3];

                for(int rows = 0; rows < val_howmanyrows; ++rows) {

                    cudaError_t err = cudaMemcpy2D(&buf_d[val_start+rows*val_striderows], sizeof(buf_t),
                                                   &recvBuffer[haloId][bufferId*haloSize + mpiRecvBufferIndex], sizeof(buf_t),
                                                   sizeof(buf_t), val_howmanycols, cudaMemcpyHostToDevice);
                    if(err != cudaSuccess)
                        std::cout << "Tausch::unpackRecvBufferCUDA(): CUDA error detected: " << err << std::endl;

                    mpiRecvBufferIndex += val_howmanycols;

                }

            }

        }

    }

#endif

    inline std::vector<std::array<int, 4> > extractHaloIndicesWithStride(std::vector<int> indices) {

        // nothing to do
        if(indices.size() == 0)
            return std::vector<std::array<int, 4> >();

        // first we build a collection of all consecutive rows
        std::vector<std::array<int, 2> > rows;

        int curIndex = 1;
        int start = indices[0];
        int howmany = 1;
        while(curIndex < indices.size()) {

            if(indices[curIndex]-indices[curIndex-1] == 1)
                ++howmany;
            else {

                rows.push_back({start, howmany});

                start = indices[curIndex];
                howmany = 1;

            }

            ++curIndex;

        }

        rows.push_back({start, howmany});

        // second we look for simple patterns within these rows
        std::vector<std::array<int, 4> > ret;

        ret.push_back({rows[0][0], rows[0][1], 1, 0});

        for(int currow = 1; currow < rows.size(); ++currow) {

            if(rows[currow][1] == ret.back()[1] && (ret.back()[3] == 0 || rows[currow][0]-(ret.back()[0]+(ret.back()[2]-1)*ret.back()[3]) == ret.back()[3])) {

                if(ret.back()[3] == 0) {
                    ++ret.back()[2];
                    ret.back()[3] = rows[currow][0]-ret.back()[0];
                } else
                    ++ret.back()[2];

            } else {

                ret.push_back({rows[currow][0], rows[currow][1], 1, 0});
            }

        }

        return ret;

    }

    MPI_Comm TAUSCH_COMM;
    MPI_Datatype mpiDataType;

    std::vector<std::vector<std::array<int, 4> > > localHaloIndices;
    std::vector<std::vector<std::array<int, 4> > > remoteHaloIndices;

    std::vector<size_t> localHaloIndicesSize;
    std::vector<size_t> remoteHaloIndicesSize;

    std::vector<int> localHaloRemoteMpiRank;
    std::vector<int> remoteHaloRemoteMpiRank;

    std::vector<size_t> localHaloNumBuffers;
    std::vector<size_t> remoteHaloNumBuffers;

    std::vector<std::unique_ptr<buf_t[]> > sendBuffer;
    std::vector<std::unique_ptr<buf_t[]> > recvBuffer;

    std::vector<MPI_Datatype> sendDatatype;
    std::vector<MPI_Datatype> recvDatatype;

    std::vector<std::unique_ptr<MPI_Request> > mpiSendRequests;
    std::vector<std::unique_ptr<MPI_Request> > mpiRecvRequests;

    std::vector<bool> setupMpiSend;
    std::vector<bool> setupMpiRecv;

    std::vector<TauschOptimizationHint> localOptHints;
    std::vector<TauschOptimizationHint> remoteOptHints;

    std::vector<bool> useMpiDerivedDatatype;


    // this is used for exchanges on same mpi rank
    std::map<int, int> msgtagToHaloId;

#ifdef TAUSCH_OPENCL

    cl::Device device;
    cl::Context context;
    cl::CommandQueue queue;
    cl::Program programs;
    int clKernelLocalSize;
    std::string cName4BufT;

#endif

#ifdef TAUSCH_CUDA
    std::map<int, buf_t*> sendCommunicationBufferKeptOnCuda;
    std::map<int, buf_t*> recvCommunicationBufferKeptOnCuda;
#endif

};


#endif
