#include "crow/crow.h"
#include <atomic>
#include <algorithm>
#include <thread>
#include <random>
#include <chrono>

#define MAX_TICKERS 1024
#define MAX_ORDERS 1000
std::atomic<bool> autoSimulating{false};

struct Order {
    int price;
    int quantity;
};

Order buyOrders[MAX_TICKERS][MAX_ORDERS];
Order sellOrders[MAX_TICKERS][MAX_ORDERS];

std::atomic<int> buyCount[MAX_TICKERS];
std::atomic<int> sellCount[MAX_TICKERS];

void addOrder(bool isBuy, int ticker, int quantity, int price) {
    if (ticker < 0 || ticker >= MAX_TICKERS) return;

    Order newOrder{price, quantity};

    if (isBuy) {
        int idx = buyCount[ticker].fetch_add(1);
        if (idx < MAX_ORDERS)
            buyOrders[ticker][idx] = newOrder;
    } else {
        int idx = sellCount[ticker].fetch_add(1);
        if (idx < MAX_ORDERS)
            sellOrders[ticker][idx] = newOrder;
    }
}

crow::json::wvalue matchOrders(int ticker) {
    crow::json::wvalue res;
    crow::json::wvalue::list matches;

    int buyIdx = buyCount[ticker].load();
    int sellIdx = sellCount[ticker].load();

    for (int i = 0; i < buyIdx; ++i) {
        Order &buyOrder = buyOrders[ticker][i];
        if (buyOrder.quantity == 0) continue;

        for (int j = 0; j < sellIdx; ++j) {
            Order &sellOrder = sellOrders[ticker][j];
            if (sellOrder.quantity == 0) continue;

            if (buyOrder.price >= sellOrder.price) {
                int matchedQty = std::min(buyOrder.quantity, sellOrder.quantity);
                buyOrder.quantity -= matchedQty;
                sellOrder.quantity -= matchedQty;

                crow::json::wvalue matchEntry;
                matchEntry["buyPrice"] = buyOrder.price;
                matchEntry["sellPrice"] = sellOrder.price;
                matchEntry["quantity"] = matchedQty;
                matchEntry["ticker"] = ticker;

                matches.push_back(std::move(matchEntry));

                if (buyOrder.quantity == 0) break;
            }
        }
    }

    res["status"] = "Matching complete";
    res["matches"] = std::move(matches);

    return res;
}

void simulateOrders() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> ticker_dist(0, MAX_TICKERS - 1);
    std::uniform_int_distribution<> price_dist(90, 110);
    std::uniform_int_distribution<> qty_dist(1, 10);
    std::uniform_int_distribution<> type_dist(0, 1);

    while (true) {
        if (autoSimulating.load()) {
            int ticker = ticker_dist(gen);
            int price = price_dist(gen);
            int quantity = qty_dist(gen);
            bool isBuy = type_dist(gen);

            addOrder(isBuy, ticker, quantity, price);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

struct CORS {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 204;
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response& res, context&) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
    }
};


int main() {
    for (int i = 0; i < MAX_TICKERS; ++i) {
        buyCount[i] = 0;
        sellCount[i] = 0;
    }

    crow::App<CORS> app;
    std::thread(simulateOrders).detach();

    CROW_ROUTE(app, "/")([] {
        return "✅ Real-time Trading Engine running!";
    });

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
        res["type"] = isBuy ? "Buy" : "Sell";
        res["price"] = price;
        res["quantity"] = quantity;

        crow::response response(res);
        
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/matchOrder").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
       if (!body) {
    crow::response response(400);
   
    response.body = "Invalid JSON";
    return response;
}


        int ticker = body["ticker"].i();

        crow::json::wvalue res = matchOrders(ticker);
        crow::response response(res);
        
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/addRandomOrders").methods("POST"_method)([] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> ticker_dist(0, MAX_TICKERS - 1);
    std::uniform_int_distribution<> price_dist(90, 110);
    std::uniform_int_distribution<> qty_dist(1, 10);
    std::uniform_int_distribution<> type_dist(0, 1);

    for (int i = 0; i < 10; ++i) {
        int ticker = ticker_dist(gen);
        int price = price_dist(gen);
        int quantity = qty_dist(gen);
        bool isBuy = type_dist(gen);
        addOrder(isBuy, ticker, quantity, price);
    }

    crow::json::wvalue res;
    res["status"] = "10 random orders added";

    crow::response response(res);
   
    response.set_header("Content-Type", "application/json");
    return response;
});

    CROW_ROUTE(app, "/getOrders").methods("GET"_method)([] {
        crow::json::wvalue res;
        crow::json::wvalue::list buyOrderList;
        crow::json::wvalue::list sellOrderList;

        for (int ticker = 0; ticker < MAX_TICKERS; ++ticker) {
            int buyTotal = buyCount[ticker].load();
            for (int i = 0; i < buyTotal; ++i) {
                const Order& order = buyOrders[ticker][i];
               if (order.quantity >= 0) {
    crow::json::wvalue o;
    o["ticker"] = ticker;
    o["price"] = order.price;
    o["quantity"] = order.quantity;
    o["type"] = "Buy"; // NEW FIELD
    buyOrderList.push_back(std::move(o));
}

            }

            int sellTotal = sellCount[ticker].load();
            for (int i = 0; i < sellTotal; ++i) {
                const Order& order = sellOrders[ticker][i];
                if (order.quantity >= 0) {
    crow::json::wvalue o;
    o["ticker"] = ticker;
    o["price"] = order.price;
    o["quantity"] = order.quantity;
    o["type"] = "Sell"; // NEW FIELD
    sellOrderList.push_back(std::move(o));
}



            }
        }

        res["buyOrders"] = std::move(buyOrderList);
        res["sellOrders"] = std::move(sellOrderList);
        res["status"] = "Orders fetched successfully";

        crow::response response(res);
       
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/toggleAutoSim").methods("POST"_method)([] {
        autoSimulating = !autoSimulating.load();
        crow::json::wvalue res;
        res["autoSimulating"] = autoSimulating.load();
        res["status"] = autoSimulating ? "Simulation started" : "Simulation paused";

        crow::response response(res);
       
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/loadSampleOrders").methods("POST"_method)([] {
    // Clear all current orders
    for (int i = 0; i < MAX_TICKERS; ++i) {
        buyCount[i] = 0;
        sellCount[i] = 0;
    }

    // Add sample orders that will definitely match
    addOrder(true, 42, 5, 105);   // Buy TICK42 @ 105 Qty 5
    addOrder(false, 42, 3, 100);  // Sell TICK42 @ 100 Qty 3
    addOrder(false, 42, 2, 104);  // Sell TICK42 @ 104 Qty 2

    addOrder(true, 7, 10, 90);    // Buy TICK7 @ 90 Qty 10
    addOrder(false, 7, 5, 85);    // Sell TICK7 @ 85 Qty 5

    crow::json::wvalue res;
    res["status"] = "Sample orders loaded";

    crow::response response(res);
    
    response.set_header("Content-Type", "application/json");
    return response;
});


    app.port(18080).multithreaded().run();
}
