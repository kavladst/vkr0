#include <iostream>
#include <vector>
#include <fstream>
#include <future>

#include "zlib.h"

size_t MAXP2 = UINT_MAX - (UINT_MAX >> 1);

size_t BLOCK = 131072;
int LEVEL = Z_DEFAULT_COMPRESSION;


struct Buffer {
    std::vector<char> buf;
    size_t len;
};

Buffer get_header(std::ofstream &file, const std::string &input_filename, int level) {
    std::vector<char> buf;

    buf.push_back(31);
    buf.push_back(139);
    buf.push_back(8);
    buf.push_back(8);

    // TODO: time
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);

    buf.push_back(level >= 9 ? 2 : level == 1 ? 4 : 0);

    buf.push_back(3);

    buf.insert(buf.end(), input_filename.begin(), input_filename.end());

    buf.push_back(0);

    return Buffer{buf, buf.size()};
}

Buffer get_trailer(uintmax_t ulen, unsigned long check) {
    std::vector<char> buf;

    // TODO: bigendian littleendian
    for (size_t i = 0; i < 4; ++i) {
        buf.push_back((check >> 8 * i) & 0xff);
    }
    for (size_t i = 0; i < 4; ++i) {
        buf.push_back((ulen >> 8 * i) & 0xff);
    }

    return Buffer{buf, buf.size()};
}

void deflate_engine(z_stream *strm, Bytef *out_buf, size_t &out_len, int flush) {
    do {
        strm->next_out = out_buf + out_len;
        strm->avail_out = UINT_MAX;
        (void) deflate(strm, flush);
        out_len = (size_t) (strm->next_out - out_buf);
    } while (strm->avail_out == 0);
}

Buffer compress_slice(char *in_buf, size_t in_len, bool last_block_flag) {
    // TODO: убрать константный размер. Нужно увеличивать размер в при его не хватке, например при
    // strm->avail_out > 0 в deflate_engine
    std::vector<char> out_v(131122 * 2);
    auto *out_buf = (Bytef *) (&out_v.front());
    size_t out_len = 0;

    auto strategy = Z_DEFAULT_STRATEGY;

    z_stream strm;
    strm.zfree = Z_NULL;
    strm.zalloc = Z_NULL;
    strm.opaque = Z_NULL;

    int ret = deflateInit2(&strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    // TODO: добавить try catch
    if (ret != Z_OK)
        throw (EINVAL, "internal error");

    (void) deflateReset(&strm);
    (void) deflateParams(&strm, LEVEL, strategy);

    strm.next_in = (Bytef *) in_buf;
    strm.avail_in = in_len;

    if (!last_block_flag) {
        deflate_engine(&strm, out_buf, out_len, Z_BLOCK);
        int bits;
        (void) deflatePending(&strm, Z_NULL, &bits);
        deflate_engine(&strm, out_buf, out_len, Z_SYNC_FLUSH);
        deflate_engine(&strm, out_buf, out_len, Z_FULL_FLUSH);
    } else {
        deflate_engine(&strm, out_buf, out_len, Z_FINISH);
    }

    deflateEnd(&strm);

    return Buffer{out_v, out_len};
}

void parallel_compress(const std::string &filename, char *in_buf, size_t in_len) {
    std::ofstream ofs;
    ofs.open(filename + ".gz", std::ios::binary);

    auto header = get_header(ofs, filename, LEVEL);
    ofs.write(&header.buf.front(), header.len);

    std::vector<std::future<Buffer>> fs;
    size_t in_remaining_len = in_len;
    size_t check = crc32_z(0L, Z_NULL, 0);
    while (in_remaining_len > 0) {
        size_t curr_block = std::min(BLOCK, in_remaining_len);

        char *curr_in_buff = in_buf + (in_len - in_remaining_len);

        fs.push_back(std::async(
                compress_slice,
                curr_in_buff, curr_block,
                curr_block == in_remaining_len
        ));

        // TODO: нужно параллелить?
        size_t curr_check = crc32_z(0L, Z_NULL, 0);
        size_t len = curr_block;
        auto *check_buff = (Bytef *) curr_in_buff;
        while (len > MAXP2) {
            curr_check = crc32_z(curr_check, check_buff, MAXP2);
            len -= MAXP2;
            check_buff += MAXP2;
        }
        curr_check = crc32_z(curr_check, check_buff, len);

        check = crc32_combine(check, curr_check, curr_block);

        in_remaining_len -= curr_block;
    }

    for (auto &f: fs) {
        auto res = f.get();
        ofs.write(&res.buf.front(), res.len);
    }

    auto trailer = get_trailer(in_len, check);
    ofs.write(&trailer.buf.front(), trailer.len);

    ofs.close();
}

void compress(const std::string &filename) {
    std::ifstream ifs(filename);
    std::string in((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    in.c_str();

    parallel_compress(filename, &in.front(), in.size());

    ifs.close();
}

int main() {
    compress("txtbig.txt");
    return 0;
}
