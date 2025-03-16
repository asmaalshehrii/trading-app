#include "crow/crow.h"
#include <atomic>
#include <algorithm>
#include <thread>
#include <random>
#include <chrono>

#define MAX_TICKERS 1024
#define MAX_ORDERS 1000

struct Order {
    int price;
    int quantity;
};

Order buyOrders[MAX_TICKERS][MAX_ORDERS];
Order sellOrders[MAX_TICKERS][MAX_ORDERS];

std::atomic<int> buyCount[MAX_TICKERS];
std::atomic<int> sellCount[MAX_TICKERS];

// Add Buy/Sell order in a lock-free way
void addOrder(bool isBuy, int ticker, int quantity, int price) {
    if (ticker < 0 || ticker >= MAX_TICKERS) return;

    Order newOrder = { price, quantity };

    if (isBuy) {
        int index = buyCount[ticker].fetch_add(1);
        if (index < MAX_ORDERS)
            buyOrders[ticker][index] = newOrder;
    } else {
        int index = sellCount[ticker].fetch_add(1);
        if (index < MAX_ORDERS)
            sellOrders[ticker][index] = newOrder;
    }
}

// Match Buy/Sell orders (O(n) time, lock-free)
crow::json::wvalue matchOrders(int ticker) {
    crow::json::wvalue result;
    crow::json::wvalue matches;
    int matchIndex = 0;

    int buys = buyCount[ticker].load();
    int sells = sellCount[ticker].load();

    for (int i = 0; i < buys; ++i) {
        if (buyOrders[ticker][i].quantity == 0) continue;

        for (int j = 0; j < sells; ++j) {
            if (sellOrders[ticker][j].quantity == 0) continue;

            if (buyOrders[ticker][i].price >= sellOrders[ticker][j].price) {
                int matchedQty = std::min(buyOrders[ticker][i].quantity, sellOrders[ticker][j].quantity);

                buyOrders[ticker][i].quantity -= matchedQty;
                sellOrders[ticker][j].quantity -= matchedQty;

                crow::json::wvalue m;
                m["price"] = sellOrders[ticker][j].price;
                m["quantity"] = matchedQty;

                matches[matchIndex++] = std::move(m);

                if (buyOrders[ticker][i].quantity == 0) break;
            }
        }
    }

    result["matches"] = std::move(matches);
    result["status"] = "Matching complete";
    return result;
}

// Background simulator: generates random orders like a real exchange
void simulateOrders() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> ticker_dist(0, MAX_TICKERS - 1);
    std::uniform_int_distribution<> price_dist(90, 110);
    std::uniform_int_distribution<> qty_dist(1, 10);
    std::uniform_int_distribution<> type_dist(0, 1);

    while (true) {
        int ticker = ticker_dist(gen);
        int price = price_dist(gen);
        int quantity = qty_dist(gen);
        bool isBuy = type_dist(gen);

        addOrder(isBuy, ticker, quantity, price);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

struct CORS {
    struct context {}; // required by Crow's middleware system

    void before_handle(crow::request& req, crow::response& res, context&) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");

        if (req.method == crow::HTTPMethod::Options) {
            res.code = 204; // No Content
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response& res, context&) {
        // Optional: Add additional headers here
    }
};



int main() {
    crow::App<CORS> app;


    // Start background simulation thread
    std::thread(simulateOrders).detach();

    // Health check route
    CROW_ROUTE(app, "/")([] {
        return "✅ Real-time Trading Engine running!";
    });

    // POST /addOrder
    CROW_ROUTE(app, "/addOrder").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        int ticker = body["ticker"].i();
        int price = body["price"].i();
        int quantity = body["quantity"].i();
        std::string type = body["type"].s();
        bool isBuy = (type == "Buy");

        addOrder(isBuy, ticker, quantity, price);

        crow::json::wvalue res;
        res["status"] = "Order added";
        res["ticker"] = ticker;
        res["type"] = type;
        res["price"] = price;
        res["quantity"] = quantity;

        return crow::response(res);
    });

    // POST /matchOrder
    CROW_ROUTE(app, "/matchOrder").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        int ticker = body["ticker"].i();
        if (ticker < 0 || ticker >= MAX_TICKERS)
            return crow::response(400, "Invalid ticker");

        auto res = matchOrders(ticker);
        return crow::response(res);
    });

    // POST /addRandomOrders
CROW_ROUTE(app, "/addRandomOrders").methods("POST"_method)([] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> ticker_dist(0, MAX_TICKERS - 1);
    std::uniform_int_distribution<> price_dist(90, 110);
    std::uniform_int_distribution<> qty_dist(1, 10);
    std::uniform_int_distribution<> type_dist(0, 1);

    // Generate 100 random orders
    for (int i = 0; i < 100; ++i) {
        int ticker = ticker_dist(gen);
        int price = price_dist(gen);
        int quantity = qty_dist(gen);
        bool isBuy = type_dist(gen);

        addOrder(isBuy, ticker, quantity, price);
    }

    crow::json::wvalue res;
    res["status"] = "100 random orders added";
    return crow::response(res);
});


    app.port(18080).multithreaded().run();
}
