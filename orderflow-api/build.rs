fn main() {
    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .file("../ffi/src/orderbook_c.cpp")
        .file("../src/OrderBook.cpp")
        .include("../ffi/include")
        .include("../include")
        .warnings(true)
        .compile("orderflow_c");
}
