#include <catch2/catch.hpp>
#define TAUSCH_OPENCL
#define TAUSCH_CUDA
#include "../../tausch.h"
#include "ocl.h"

TEST_CASE("1 buffer, empty indices, with pack/unpack, same MPI rank") {

    setupOpenCL();

    const std::vector<int> sizes = {3, 10, 100, 377};
    const std::vector<int> halowidths = {1, 2, 3};

    for(auto size : sizes) {

        for(auto halowidth : halowidths) {

            double *in = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            double *out = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            for(int i = 0; i < size; ++i) {
                for(int j = 0; j < size; ++j) {
                    in[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                    out[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                }
            }
            double *cuda_in;
            cudaMalloc(&cuda_in, (size+2*halowidth)*(size+2*halowidth)*sizeof(double));
            cudaMemcpy(cuda_in, in, (size+2*halowidth)*(size+2*halowidth)*sizeof(double), cudaMemcpyHostToDevice);
            cl::Buffer cl_out(tauschcl_queue, out, &out[(size+2*halowidth)*(size+2*halowidth)], false);

            std::vector<int> sendIndices;
            std::vector<int> recvIndices;

            Tausch *tausch = new Tausch(MPI_COMM_WORLD, false);
            tausch->setOpenCL(tauschcl_device, tauschcl_context, tauschcl_queue);

            int mpiRank;
            MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);

            tausch->addSendHaloInfo(sendIndices, sizeof(double));
            tausch->addRecvHaloInfo(recvIndices, sizeof(double));

            tausch->packSendBufferCUDA(0, 0, cuda_in);
            tausch->send(0, 0, mpiRank);
            tausch->recv(0, 0, mpiRank);
            tausch->unpackRecvBufferOCL(0, 0, cl_out);

            cl::copy(tauschcl_queue, cl_out, out, &out[(size+2*halowidth)*(size+2*halowidth)]);

            double *expected = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            for(int i = 0; i < size; ++i) {
                for(int j = 0; j < size; ++j) {
                    expected[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                }
            }

            // check result
            for(int i = 0; i < (size+2*halowidth); ++i)
                for(int j = 0; j < (size+2*halowidth); ++j)
                    REQUIRE(expected[i*(size+2*halowidth)+j] == out[i*(size+2*halowidth)+j]);

        }

    }

}

TEST_CASE("1 buffer, empty indices, with pack/unpack, multiple MPI ranks") {

    setupOpenCL();

    const std::vector<int> sizes = {3, 10, 100, 377};
    const std::vector<int> halowidths = {1, 2, 3};

    for(auto size : sizes) {

        for(auto halowidth : halowidths) {

            double *in = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            double *out = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            for(int i = 0; i < size; ++i) {
                for(int j = 0; j < size; ++j) {
                    in[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                    out[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                }
            }
            double *cuda_in;
            cudaMalloc(&cuda_in, (size+2*halowidth)*(size+2*halowidth)*sizeof(double));
            cudaMemcpy(cuda_in, in, (size+2*halowidth)*(size+2*halowidth)*sizeof(double), cudaMemcpyHostToDevice);
            cl::Buffer cl_out(tauschcl_queue, out, &out[(size+2*halowidth)*(size+2*halowidth)], false);

            std::vector<int> sendIndices;
            std::vector<int> recvIndices;

            Tausch *tausch = new Tausch(MPI_COMM_WORLD, false);
            tausch->setOpenCL(tauschcl_device, tauschcl_context, tauschcl_queue);

            int mpiRank, mpiSize;
            MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
            MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);

            tausch->addSendHaloInfo(sendIndices, sizeof(double));
            tausch->addRecvHaloInfo(recvIndices, sizeof(double));

            tausch->packSendBufferCUDA(0, 0, cuda_in);
            tausch->send(0, 0, (mpiRank+1)%mpiSize);
            tausch->recv(0, 0, (mpiRank+mpiSize-1)%mpiSize);
            tausch->unpackRecvBufferOCL(0, 0, cl_out);

            cl::copy(tauschcl_queue, cl_out, out, &out[(size+2*halowidth)*(size+2*halowidth)]);

            double *expected = new double[(size+2*halowidth)*(size+2*halowidth)]{};
            for(int i = 0; i < size; ++i) {
                for(int j = 0; j < size; ++j) {
                    expected[(i+halowidth)*(size+2*halowidth) + j+halowidth] = i*size + j + 1;
                }
            }

            // check result
            for(int i = 0; i < (size+2*halowidth); ++i)
                for(int j = 0; j < (size+2*halowidth); ++j)
                    REQUIRE(expected[i*(size+2*halowidth)+j] == out[i*(size+2*halowidth)+j]);

        }

    }

}
