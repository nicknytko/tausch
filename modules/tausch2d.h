/*!
 * \file
 * \author  Lukas Spies <LSpies@illinois.edu>
 * \version 1.0
 */

#ifndef TAUSCH2D_H
#define TAUSCH2D_H

#include <mpi.h>
#include <fstream>
#include <cmath>
#include <sstream>
#include <iostream>
#include <thread>
#include <future>

#ifdef TAUSCH_OPENCL
    #define __CL_ENABLE_EXCEPTIONS
    #include <CL/cl.hpp>
#endif

/*!
 * Use real_t in code to allow easier switch between double/float.
 */
typedef double real_t;

/*!
 *
 * \brief
 *  A library providing a clean and efficient interface for halo exchange in two dimensions.
 *
 * %Tausch2D is a library that provides a clean and efficient C and C++ API for halo exchange for two dimensional structured grids that are split into a structured coarse mesh for MPI. It supports halo exchange across the partition boundaries, and across a CPU/GPU boundary for GPU partitions living centered inside a CPU partition.
 */
class Tausch2D {

public:

    /*!
     * These are the edges available for inter-MPI halo exchanges: LEFT, RIGHT, TOP, BOTTOM.
     */
    enum Edge { LEFT, RIGHT, TOP, BOTTOM };

    /*!
     * These are the two dimensions used, used for clarity as to which array entry is which dimension: X, Y.
     */
    enum Dimensions { X, Y };

    /*!
     *
     * The default class constructor, expecting some basic yet important details about the data.
     *
     * \param localDim
     *  Array of size 2 holding the dimensions of the local partition (not the global dimensions), with the x dimension being the first value and the y dimension being the second one.
     * \param mpiNum
     *  Array of size 2 holding the number of MPI ranks lined up in the x (first value) and y (second value) direction. mpiNumX*mpiNumY has to be equal to the total number of MPI ranks.
     * \param cpuHaloWidth
     *  Array of size 4 containing the widths of the CPU-to-CPU halos, i.e., the inter-MPI halo. The order in which the halo widths are expected to be stored is: LEFT -> RIGHT -> TOP -> BOTTOM
     * \param comm
     *  The MPI Communictor to be used. %Tausch2D will duplicate the communicator, thus it is safe to have multiple instances of %Tausch2D working with the same communicator. By default, MPI_COMM_WORLD will be used.
     */
    Tausch2D(int *localDim, int *mpiNum, int *cpuHaloWidth, MPI_Comm comm = MPI_COMM_WORLD);

    /*!
     *
     * Overloaded constructor, allowing specification of CPU halo using single integer.
     *
     * \param localDim
     *  Array of size 2 holding the dimensions of the local partition (not the global dimensions), with the x dimension being the first value and the y dimension being the second one.
     * \param mpiNum
     *  Array of size 2 holding the number of MPI ranks lined up in the x (first value) and y (second value) direction. mpiNumX*mpiNumY has to be equal to the total number of MPI ranks.
     * \param cpuHaloWidth
     *  Width of the CPU-to-CPU halos, i.e., the inter-MPI halo. Will be used for all four halo widths.
     * \param comm
     *  The MPI Communictor to be used. %Tausch2D will duplicate the communicator, thus it is safe to have multiple instances of %Tausch2D working with the same communicator. By default, MPI_COMM_WORLD will be used.
     */
    Tausch2D(int *localDim, int *mpiNum, int cpuHaloWidth, MPI_Comm comm = MPI_COMM_WORLD);

    /*!
     * Destructor freeing any allocated memory.
     */
    ~Tausch2D();

    /*!
     * Tells %Tausch2D where to find the buffer for the CPU data.
     *
     * \param data
     *  The buffer holding the CPU data. This is expected to be one contiguous buffer holding both the values owned by this MPI rank and the ghost values.
     */
    void setCPUData(real_t *data);

    /*!
     * Tells %Tausch2D where to find the buffer for the CPU stencil data.
     *
     * \param stencil
     *  The buffer holding the CPU stencil data. This is expected to be one contiguous buffer holding both the values owned by this MPI rank and the ghost values.
     * \param stencilNumPoints
     *  The number of points in the stencil. If the storage makes use of symmetry, this is expected to be the effective number of stored stencil values.
     *
     */
    void setCPUStencil(real_t *stencil, int stencilNumPoints);

    /*!
     * Post the MPI_Irecv required for the halo exchange. This has to be called before any halo exchange is started.
     */
    void postCpuDataReceives();

    /*!
     * Post the MPI_Irecv required for the stencil halo exchange. This has to be called before any stencil halo exchange is started.
     */
    void postCpuStencilReceives();

    /*!
     * Convenience function that calls the necessary functions performing a halo exchange across MPI ranks. After posting the MPI_Recvs, it first calls a left/right halo exchange followed by a top/bottom halo exchange.
     */
    void performCpuToCpuData() { postCpuDataReceives();
                                 startCpuDataEdge(LEFT); startCpuDataEdge(RIGHT); completeCpuDataEdge(LEFT); completeCpuDataEdge(RIGHT);
                                 startCpuDataEdge(TOP); startCpuDataEdge(BOTTOM); completeCpuDataEdge(TOP); completeCpuDataEdge(BOTTOM); }

    /*!
     * Convenience function that calls the necessary functions performing a stencil halo exchange across MPI ranks. After posting the MPI_Recvs, it first calls a left/right stencil halo exchange followed by a top/bottom halo exchange.
     */
   void performCpuToCpuStencil() { postCpuStencilReceives();
                                   startCpuStencilEdge(LEFT); startCpuStencilEdge(RIGHT); completeCpuStencilEdge(LEFT); completeCpuStencilEdge(RIGHT);
                                   startCpuStencilEdge(TOP); startCpuStencilEdge(BOTTOM); completeCpuStencilEdge(TOP); completeCpuStencilEdge(BOTTOM); }

    /*!
     * Start the inter-MPI halo exchange across the given edge.
     *
     * \param edge
     *  The edge across which the halo exchange is supposed to be started. It can be either one of the enum Tausch2D::LEFT, Tausch2D::RIGHT, Tausch2D::TOP, Tausch2D::BOTTOM.
     */
    void startCpuDataEdge(Edge edge);

    /*!
     * Start the inter-MPI stencil halo exchange across the given edge.
     *
     * \param edge
     *  The edge across which the stencil halo exchange is supposed to be started. It can be either one of the enum Tausch2D::LEFT, Tausch2D::RIGHT, Tausch2D::TOP, Tausch2D::BOTTOM.
     */
    void startCpuStencilEdge(Edge edge);

    /*!
     * Completes the inter-MPI halo exchange across the given edge. This has to come *after* calling startCpuEdge() on the same edge.
     *
     * \param edge
     *  The edge across which the halo exchange is supposed to be completed. It can be either one of the enum Tausch2D::LEFT, Tausch2D::RIGHT, Tausch2D::TOP, Tausch2D::BOTTOM.
     */
    void completeCpuDataEdge(Edge edge);

    /*!
     * Completes the inter-MPI stencil halo exchange across the given edge. This has to come *after* calling startCpuStencilEdge() on the same edge.
     *
     * \param edge
     *  The edge across which the stencil halo exchange is supposed to be completed. It can be either one of the enum Tausch2D::LEFT, Tausch2D::RIGHT, Tausch2D::TOP, Tausch2D::BOTTOM.
     */
    void completeCpuStencilEdge(Edge edge);

    /*!
     * Get the MPI communicator that %Tausch2D uses for all of its communication. Be careful not to 'overwrite' MPI tags that %Tausch2D uses.
     *
     * \return
     *  Returns the MPI communicator used by %Tausch2D.
     */
    MPI_Comm getMPICommunicator() { return TAUSCH_COMM; }

#ifdef TAUSCH_OPENCL

    /*!
     * Enable OpenCL for the current %Tausch2D object. This causes %Tausch2D to set up its own OpenCL environment.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param gpuHaloWidth
     *  Array of size 4 holding the widths of the CPU/GPU halo. The values are expected in the edge order: LEFT -> RIGHT -> TOP -> BOTTOM.
     * \param blockingSyncCpuGpu
     *  Whether to sync the CPU and GPU. This is necessary when running both parts in asynchronous threads, but causes a deadlock when only one thread is used. Default value: true
     * \param clLocalWorkgroupSize
     *  The local workgroup size for each kernel call. Default value: 64
     * \param giveOpenCLDeviceName
     *  Whether %Tausch2D should print out the OpenCL device name. This can come in handy for debugging. Default value: false
     */
    void enableOpenCL(int *gpuHaloWidth, bool blockingSyncCpuGpu = true, int clLocalWorkgroupSize = 64, bool giveOpenCLDeviceName = false);

    /*!
     * Overloaded function, allowing specification of GPU halo using single integer.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param gpuHaloWidth
     *  The width of the CPU/GPU halo, taken as width for all four edges.
     * \param blockingSyncCpuGpu
     *  Whether to sync the CPU and GPU. This is necessary when running both parts in asynchronous threads, but causes a deadlock when only one thread is used. Default value: true
     * \param clLocalWorkgroupSize
     *  The local workgroup size for each kernel call. Default value: 64
     * \param giveOpenCLDeviceName
     *  Whether %Tausch2D should print out the OpenCL device name. This can come in handy for debugging. Default value: false
     */
    void enableOpenCL(int gpuHaloWidth, bool blockingSyncCpuGpu = true, int clLocalWorkgroupSize = 64, bool giveOpenCLDeviceName = false);

    /*!
     * Overloaded function, enabling OpenCL for the current %Tausch2D object, making %Tausch2D use the user-provided OpenCL environment.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param cl_defaultDevice
     *  The OpenCL device.
     * \param cl_context
     *  The OpenCL context
     * \param cl_queue
     *  The OpenCL queue
     * \param gpuHaloWidth
     *  Array of size 4 holding the widths of the CPU/GPU halo. The values are expected in the edge order: LEFT -> RIGHT -> TOP -> BOTTOM.
     * \param blockingSyncCpuGpu
     *  Whether to sync the CPU and GPU. This is necessary when running both parts in asynchronous threads, but causes a deadlock when only one thread is used. Default value: true
     * \param clLocalWorkgroupSize
     *  The local workgroup size for each kernel call. Default value: 64
     */
    void enableOpenCL(cl::Device &cl_defaultDevice, cl::Context &cl_context, cl::CommandQueue &cl_queue, int *gpuHaloWidth, bool blockingSyncCpuGpu = true, int clLocalWorkgroupSize = 64);

    /*!
     * Overloaded function, allowing specification of GPU halo using single integer.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param cl_defaultDevice
     *  The OpenCL device.
     * \param cl_context
     *  The OpenCL context
     * \param cl_queue
     *  The OpenCL queue
     * \param gpuHaloWidth
     *  The width of the CPU/GPU halo, taken as width for all four edges.
     * \param blockingSyncCpuGpu
     *  Whether to sync the CPU and GPU. This is necessary when running both parts in asynchronous threads, but causes a deadlock when only one thread is used. Default value: true
     * \param clLocalWorkgroupSize
     *  The local workgroup size for each kernel call. Default value: 64
     */
    void enableOpenCL(cl::Device &cl_defaultDevice, cl::Context &cl_context, cl::CommandQueue &cl_queue, int gpuHaloWidth, bool blockingSyncCpuGpu = true, int clLocalWorkgroupSize = 64);

    /*!
     * Tells %Tausch2D where to find the buffer for the GPU data and some of its main important details.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param dat
     *  The OpenCL buffer holding the GPU data. This is expected to be one contiguous buffer holding both the values owned by this GPU and the ghost values.
     * \param gpuDim
     *  Array of size 2, holding the x (first value) and y (second value) dimensions of the GPU buffer.
     */
    void setGPUData(cl::Buffer &dat, int *gpuDim);

    /*!
     * Tells %Tausch2D where to find the buffer for the GPU stencil and some of its main important details.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \param stencil
     *  The OpenCL buffer holding the GPU stencil data. This is expected to be one contiguous buffer holding both the values owned by this GPU and the ghost values.
     * \param stencilNumPoints
     *  The number of points in the stencil. If the storage makes use of symmetry, this is expected to be the effective number of stored stencil values.
     * \param stencilDim
     *  Array of size 2, holding the x (first value) and y (second value) dimensions of the GPU stencil buffer. This is typically the same as gpuDim. If stencilDim is set to {0,0} or nullptr, %Tausch2D will copy the values from gpuDim.
     */
    void setGPUStencil(cl::Buffer &stencil, int stencilNumPoints, int *stencilDim = nullptr);

    /*!
     * Convenience function that calls the necessary functions performing a halo exchange from the CPU to GPU. It calls startCpuToGpuData() and completeGpuToCpuData().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performCpuToGpuData() { startCpuToGpuData(); completeGpuToCpuData(); }

    /*!
     * Convenience function that calls the necessary functions performing a stencil halo exchange from the CPU to GPU. It calls startCpuToGpuStencil() and completeGpuToCpuStencil().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performCpuToGpuStencil() { startCpuToGpuStencil(); completeGpuToCpuStencil(); }

    /*!
     * Convenience function that calls the necessary functions performing a halo exchange across the MPI ranks and from the CPU to GPU. It interweaves MPI communication with write to shared memory for sending data to the GPU. The CPU/GPU methods called are startCpuToGpu() and completeGpuToCpu().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performCpuToCpuDataAndCpuToGpuData() { postCpuDataReceives();
                                                startCpuDataEdge(LEFT); startCpuDataEdge(RIGHT); startCpuToGpuData();
                                                completeCpuDataEdge(LEFT); completeCpuDataEdge(RIGHT); startCpuDataEdge(TOP); startCpuDataEdge(BOTTOM);
                                                completeGpuToCpuData(); completeCpuDataEdge(TOP); completeCpuDataEdge(BOTTOM); }

    /*!
     * Convenience function that calls the necessary functions performing a stencil halo exchange across the MPI ranks and from the CPU to GPU. It interweaves MPI communication with write to shared memory for sending data to the GPU. The CPU/GPU methods called are startCpuToGpuStencil() and completeGpuToCpuStencil().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performCpuToCpuStencilAndCpuToGpuStencil() { postCpuStencilReceives();
                                                startCpuStencilEdge(LEFT); startCpuStencilEdge(RIGHT); startCpuToGpuStencil();
                                                completeCpuStencilEdge(LEFT); completeCpuStencilEdge(RIGHT); startCpuStencilEdge(TOP); startCpuStencilEdge(BOTTOM);
                                                completeGpuToCpuStencil(); completeCpuStencilEdge(TOP); completeCpuStencilEdge(BOTTOM); }

    /*!
     * Convenience function that calls the necessary functions performing a halo exchange from the GPU to CPU. It calls startGpuToCpu() and completeCpuToGpu().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performGpuToCpuData() { startGpuToCpuData(); completeCpuToGpuData(); }

    /*!
     * Convenience function that calls the necessary functions performing a stencil halo exchange from the GPU to CPU. It calls startGpuToCpuStencil() and completeCpuToGpuStencil().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void performGpuToCpuStencil() { startGpuToCpuStencil(); completeCpuToGpuStencil(); }

    /*!
     * Start the halo exchange from the CPU to the GPU (called by the CPU thread). This writes the required halo data to shared memory using atomic operations.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void startCpuToGpuData();

    /*!
     * Start the stencil halo exchange from the CPU to the GPU (called by the CPU thread). This writes the required stencil halo data to shared memory using atomic operations.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void startCpuToGpuStencil();

    /*!
     * Start the halo exchange from the GPU to the CPU (called by te GPU thread). This downloads the required halo data from the GPU and writes it to shared memory using atomic operations.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void startGpuToCpuData();

    /*!
     * Start the stencil halo exchange from the GPU to the CPU (called by te GPU thread). This downloads the required stencil halo data from the GPU and writes it to shared memory using atomic operations.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void startGpuToCpuStencil();

    /*!
     * Completes the halo exchange from the CPU to the GPU (called by the GPU thread). This takes the halo data the GPU wrote to shared memory and loads it into the respective buffer positions. This has to come *after* calling startCpuToGpuData().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void completeCpuToGpuData();

    /*!
     * Completes the stencil halo exchange from the CPU to the GPU (called by the GPU thread). This takes the stencil halo data the GPU wrote to shared memory and loads it into the respective buffer positions. This has to come *after* calling startCpuToGpuStencil().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void completeCpuToGpuStencil();

    /*!
     * Completes the halo exchange from the GPU to the CPU (called by the CPU thread). This takes the halo data the CPU wrote to shared memory and uploads it to the GPU. This has to come *after* calling startGpuToCpuData().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void completeGpuToCpuData();

    /*!
     * Completes the stencil halo exchange from the GPU to the CPU (called by the CPU thread). This takes the stencil halo data the CPU wrote to shared memory and uploads it to the GPU. This has to come *after* calling startGpuToCpuStencil().
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     */
    void completeGpuToCpuStencil();

    /*!
     * Return the OpenCL context used by %Tausch2D. This can be especially useful when %Tausch2D uses its own OpenCL environment and the user wants to piggyback on that.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \return
     *  The OpenCL context used by %Tausch2D.
     */
    cl::Context getContext() { return cl_context; }
    /*!
     * Return the OpenCL command queue used by %Tausch2D. This can be especially useful when %Tausch2D uses its own OpenCL environment and the user wants to piggyback on that.
     *
     * Note: This is only available if %Tausch2D was compiled with OpenCL support!
     *
     * \return
     *  The OpenCL command queue used by %Tausch2D.
     */
    cl::CommandQueue getQueue() { return cl_queue; }

#endif

private:
    void _constructor(int *localDim, int *mpiNum, int *cpuHaloWidth, MPI_Comm comm);

    // The current MPI rank and size of TAUSCH_COMM world
    int mpiRank, mpiSize;
    // The number of MPI ranks in the x/y direction of the domain
    int mpiNum[2];

    MPI_Datatype mpiDataType;

    // The x/y dimensions of the LOCAL partition
    int localDim[2];

    // Pointer to the CPU data
    real_t *cpuData;
    real_t *cpuStencil;
    int stencilNumPoints;

    // The width of the halo
    int cpuHaloWidth[4];

    // Double pointer holding the MPI sent/recvd data across all edges
    real_t **cpuToCpuSendBuffer;
    real_t **cpuToCpuRecvBuffer;
    real_t **cpuToCpuStencilSendBuffer;
    real_t **cpuToCpuStencilRecvBuffer;

    // Whether the necessary steps were taken before starting a halo exchange
    bool cpuInfoGiven;
    bool stencilInfoGiven;
    bool cpuRecvsPosted;
    bool stencilRecvsPosted;

    // Which edge of the halo exchange has been started
    bool cpuStarted[4];
    bool cpuStencilStarted[4];

    // this refers to inter-partition boundaries
    bool haveBoundary[4];

    // The communicator in use by Tausch2D
    MPI_Comm TAUSCH_COMM;

    // Holding the MPI requests so we can call Wait on the right ones
    MPI_Request cpuToCpuSendRequest[4];
    MPI_Request cpuToCpuRecvRequest[4];
    MPI_Request cpuToCpuStencilSendRequest[4];
    MPI_Request cpuToCpuStencilRecvRequest[4];

#ifdef TAUSCH_OPENCL

    // The OpenCL Device, Context and CommandQueue in use by Tausch2D (either set up by Tausch2D or passed on from user)
    cl::Device cl_defaultDevice;
    cl::Context cl_context;
    cl::CommandQueue cl_queue;

    int gpuHaloWidth[4];

    // The OpenCL buffer holding the data on the GPU
    cl::Buffer gpuData;
    cl::Buffer gpuStencil;

    // Some meta information about the OpenCL region
    int gpuDim[2];
    int stencilDim[2];
    cl::Buffer cl_gpuDim[2];
    cl::Buffer cl_stencilDim[2];
    cl::Buffer cl_gpuHaloWidth;
    cl::Buffer cl_stencilNumPoints;

    // Methods to set up the OpenCL environment and compile the required kernels
    void setupOpenCL(bool giveOpenCLDeviceName);
    void compileKernels();

    // Pointers to atomic arrays for communicating halo data between CPU and GPU thread via shared memory
    std::atomic<real_t> *cpuToGpuDataBuffer;
    std::atomic<real_t> *gpuToCpuDataBuffer;
    std::atomic<real_t> *cpuToGpuStencilBuffer;
    std::atomic<real_t> *gpuToCpuStencilBuffer;

    // These are only needed/set when Tausch2D set up its own OpenCL environment, not needed otherwise
    cl::Platform cl_platform;
    cl::Program cl_programs;

    // Collecting the halo data to be sent/recvd
    cl::Buffer cl_gpuToCpuDataBuffer;
    cl::Buffer cl_cpuToGpuDataBuffer;
    cl::Buffer cl_gpuToCpuStencilBuffer;
    cl::Buffer cl_cpuToGpuStencilBuffer;

    // Whether the necessary steps were taken before starting a halo exchange
    bool gpuEnabled;
    bool gpuDataInfoGiven;
    bool gpuStencilInfoGiven;

    // Which halo exchange has been started
    std::atomic<bool> cpuToGpuDataStarted;
    std::atomic<bool> gpuToCpuDataStarted;
    std::atomic<bool> cpuToGpuStencilStarted;
    std::atomic<bool> gpuToCpuStencilStarted;

    // The local OpenCL workgroup size
    int cl_kernelLocalSize;

    // Function to synchronize the CPU and GPU thread. Only needed when using asynchronous computing (i.e., when blocking is enabled)
    void syncCpuAndGpu(bool offsetByTwo);
    std::atomic<int> sync_counter[4];
    std::atomic<int> sync_lock[4];

    // Whether there are two threads running and thus whether there needs to be blocking synchronisation happen
    bool blockingSyncCpuGpu;

#endif

};

#endif // TAUSCH2D_H
