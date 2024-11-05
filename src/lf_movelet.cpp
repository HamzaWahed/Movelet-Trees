#include <cstdlib>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/int_vector.hpp>
#include <sdsl/rank_support.hpp>
#include <sdsl/sd_vector.hpp>

#include <string>
#include <utility>
#include <vector>

#define CHAR_COUNT 256

int n = 0;
int r = 0;
int sigma = 0;

// all the characters in T or BWT(T)
std::vector<unsigned char> alphabet;

// to map a character to the index
// Ex. #:0, $:1, A:2, C:3, G:4, T:5
std::vector<int> char_to_index;

// The data structures needed for the assignment
sdsl::sd_vector B_F_sparse;
sdsl::sd_vector B_L_sparse;

sdsl::bit_vector B_FL;

std::vector<int> C;
std::vector<char> H_L;
std::vector<std::unique_ptr<sdsl::bit_vector>> B_x;

// Rank and Select data structures on B_F and B_L
sdsl::rank_support_sd<> rank_B_L;
sdsl::select_support_sd<> select_B_L;
sdsl::rank_support_sd<> rank_B_F;
sdsl::select_support_sd<> select_B_F;

// Rank and select data structures for B_FL
sdsl::rank_support_sd<> rank_B_FL;
sdsl::select_support_sd<> select_B_FL;

// Rank data structure for B_x bit vectors
std::vector<std::unique_ptr<sdsl::rank_support_v<>>> B_x_ranks;

void deserialize_data(char *inputFileName) {
    std::ifstream in_file(inputFileName, std::ios::in | std::ios::binary);

    in_file.read(reinterpret_cast<char *>(&n), sizeof(n));
    in_file.read(reinterpret_cast<char *>(&r), sizeof(r));
    in_file.read(reinterpret_cast<char *>(&sigma), sizeof(sigma));
    std::cerr << "n: " << n << "\n";
    std::cerr << "r: " << r << "\n";
    std::cerr << "sigma: " << sigma << "\n";

    H_L.resize(r);
    in_file.read(reinterpret_cast<char *>(&H_L[0]), r * sizeof(H_L[0]));

    C.resize(sigma);
    in_file.read(reinterpret_cast<char *>(&C[0]), sigma * sizeof(C[0]));

    alphabet.resize(sigma);
    in_file.read(reinterpret_cast<char *>(&alphabet[0]),
                 sigma * sizeof(alphabet[0]));
    std::cerr << "\nThe alphabet in the BWT:\n";
    for (int i = 0; i < sigma; i++)
        std::cerr << "\t" << i << " -> " << static_cast<int>(alphabet[i]) << "("
                  << alphabet[i] << ")\n";
    std::cerr << "\n";
    char_to_index.resize(CHAR_COUNT);
    in_file.read(reinterpret_cast<char *>(&char_to_index[0]),
                 CHAR_COUNT * sizeof(char_to_index[0]));

    B_L_sparse.load(in_file);

    // Building the rank and select data structures for querying B_L
    rank_B_L = sdsl::rank_support_sd<>(
        &B_L_sparse); // usage example: rank_B_L(i) gives the rank result at
                      // index i on B_L
    select_B_L = sdsl::select_support_sd<>(
        &B_L_sparse); // usage example: select_B_L(i) gives the select result at
                      // index i on B_L

    B_F_sparse.load(in_file);

    // Building the rank and select data structures for querying B_L
    rank_B_F = sdsl::rank_support_sd<>(
        &B_F_sparse); // usage example: rank_B_L(i) gives the rank result at
                      // index i on B_L
    select_B_F = sdsl::select_support_sd<>(
        &B_F_sparse); // usage example: select_B_L(i) gives the select result at
                      // index i on B_L

    for (int i = 0; i < sigma; i++) {
        auto new_b_vector = std::make_unique<sdsl::bit_vector>();
        new_b_vector->load(in_file);
        B_x.push_back(std::move(new_b_vector));
    }

    // create the rank objects for the B_x bit vectors
    for (auto &B : B_x) {
        B_x_ranks.emplace_back(std::unique_ptr<sdsl::rank_support_v<>>(
            new sdsl::rank_support_v<>(B.get())));
    }
    // Example: code to perform rank query on B_2 at position 10:
    // std::cerr << (*B_x_ranks[2])(10) << "\n";

    in_file.close();
}

// Builds the B_FL bitvector
sdsl::sd_vector<> build_B_FL(){
    // initialize to 2*r bits
    sdsl::bit_vector B_FL(2*r);

    // track the index in B_FL
    size_t i_BFL = 0;

    // fill appropriate values in B_FL
    auto it_BF = B_F_sparse.begin();
    for (auto it_F = B_F_sparse.begin(), it_L = B_L_sparse.begin(); it_F != B_F_sparse.end() && it_L != B_L_sparse.end(); ++it_F, ++it_L, ++it_BF) {
        if (*it_F == 1 && *it_L == 1) {
            B_FL[i_BFL] = 0;
            i_BFL++;
            B_FL[i_BFL] = 1;
            i_BFL++;
        } else if (*it_F == 1) {
            B_FL[i_BFL] = 1;
            i_BFL++;
        } else if (*it_L == 1) {
            B_FL[i_BFL] = 0;
            i_BFL++;
        }
    }

    // convert to sd_vector so we can easily utilize rank and select
    sdsl::sd_vector<> B_FL_sd = sdsl::sd_vector<>(B_FL);

    // print its value for assurance purposes
    // std::cerr << "B_FL_sd: " << B_FL_sd << "\n";

    return B_FL_sd;
} 

// We weren't able to identify a more straightforward way to get the rank of 0 in B_FL
size_t get_rank_0_BFL(sdsl::rank_support_sd<> rank_1_B_FL, size_t i) {
    return i - rank_1_B_FL(i);
}

std::pair<size_t, size_t> getRunAndOffset(size_t run_idx_F, size_t offset_idx_F) {
    
    // TODO: Figure out how to do this part
    // find LF(i) using the "traditional way"
    size_t run_head = H_L[run_idx_F];
    size_t pred_run_head = (*B_x_ranks[char_to_index[run_head]])(run_idx_F);
    size_t run_head_idx_in_F = C[char_to_index[run_head]] + pred_run_head;
    size_t idx_F =  run_head_idx_in_F + offset_idx_F;

    std::cerr << "idx_F: " << idx_F + 1 << "\n";
    // create B_FL, and rank_1 and select_1 data structures for it
    sdsl::sd_vector<> B_FL = build_B_FL();
    sdsl::rank_support_sd<> rank_1_B_FL = sdsl::rank_support_sd<>(&B_FL);
    sdsl::select_support_sd <> select_1_B_FL = sdsl::select_support_sd<>(&B_FL);
    
    // find b and l
    size_t l = select_1_B_FL(run_idx_F + 1) - (run_idx_F + 1);
    // std::cout << "l: " << l << "\n";

    // find the correct run
    while(select_B_L(l) < idx_F){
        l += 1;
    }
    l -= 2; 

    // find the correct index in that run
    size_t idx_LF_i = select_B_L(l+1);
    size_t offset_LF_i = 0;
    while(idx_LF_i < idx_F){
        idx_LF_i++;
        offset_LF_i++;
    }

    return std::make_pair(l, offset_LF_i);
}

int main(int argc, char **argv) {
    std::cerr << "usage: load DATASET.ri\n";
    std::cerr << "The DATASET.ri is generated by the build script.\n\n";

    deserialize_data(argv[1]);
    std::cerr << "All the arrays and bit vectors are loaded.\n\n";

    while (true) {
        size_t run_idx, offset_idx;
        std::cerr << "Enter run index and offset index (or -1 -1 to exit): ";
        std::cin >> run_idx >> offset_idx;

        if (run_idx == static_cast<size_t>(-1) || offset_idx == static_cast<size_t>(-1)) {
            break;
        }

        auto result = getRunAndOffset(run_idx, offset_idx);
        std::cerr << "Run index in L: " << result.first << ", Offset in L: " << result.second << "\n";
    }

    return 0;
}



