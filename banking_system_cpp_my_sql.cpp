// Project: Banking System (C++ + MySQL)
//CREATED BY OMINGLE
//IF YOU NEED ANY HELP REGAURDING THIS PROJECT YOU CAN MAIL ME
// Files contained in this single editor view:
//  - README.md
//  - db.sql
//  - Makefile
//  - include/bank.h
//  - src/bank.cpp
//  - src/main.cpp

// -------------------- README.md --------------------
/*
# Banking System (C++ + MySQL)

A simple, portfolio-ready banking application in C++ using an object-oriented design and MySQL for persistence.

## Features
- Create and manage customers and accounts
- Deposit, withdraw, transfer funds
- View account details and recent transactions
- Basic input validation and error handling

## Requirements
- g++ (C++17)
- MySQL server
- MySQL Connector/C++ (for example `libmysqlcppconn`)

On Debian/Ubuntu you can install connector (example):
```
sudo apt-get install libmysqlcppconn-dev
```

## Setup
1. Create the database and tables:
   - Run the `db.sql` script in MySQL: `mysql -u root -p < db.sql`
2. Update DB connection settings in `src/bank.cpp` (host, user, password, database)
3. Build:
```
make
```
4. Run:
```
./bank_app
```

## Notes
- This is a console application meant for learning and interviews. For production, add stronger security, transaction management, and input sanitization.
*/

// -------------------- db.sql --------------------
/*
CREATE DATABASE banking_system;
USE banking_system;

CREATE TABLE customers (
  customer_id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(100) NOT NULL,
  email VARCHAR(100),
  phone VARCHAR(20),
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE accounts (
  account_id INT AUTO_INCREMENT PRIMARY KEY,
  customer_id INT NOT NULL,
  account_type VARCHAR(20) NOT NULL,
  balance DECIMAL(15,2) DEFAULT 0.00,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (customer_id) REFERENCES customers(customer_id) ON DELETE CASCADE
);

CREATE TABLE transactions (
  transaction_id INT AUTO_INCREMENT PRIMARY KEY,
  account_id INT NOT NULL,
  type ENUM('DEPOSIT','WITHDRAW','TRANSFER') NOT NULL,
  amount DECIMAL(15,2) NOT NULL,
  details VARCHAR(255),
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (account_id) REFERENCES accounts(account_id) ON DELETE CASCADE
);
*/

// -------------------- Makefile --------------------
/*
CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -O2
LDFLAGS = -lmysqlcppconn
SRC = src/main.cpp src/bank.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = bank_app

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
*/

// -------------------- include/bank.h --------------------

#ifndef BANK_H
#define BANK_H

#include <string>
#include <vector>

struct Customer {
    int id;
    std::string name;
    std::string email;
    std::string phone;
};

struct Account {
    int id;
    int customer_id;
    std::string type;
    double balance;
};

struct TransactionRecord {
    int id;
    int account_id;
    std::string type; // DEPOSIT, WITHDRAW, TRANSFER
    double amount;
    std::string details;
    std::string created_at;
};

class Bank {
public:
    Bank(const std::string &host, const std::string &user, const std::string &pass, const std::string &db);
    ~Bank();

    // Customer operations
    int createCustomer(const std::string &name, const std::string &email, const std::string &phone);
    std::vector<Customer> listCustomers();
    Customer getCustomer(int customer_id);

    // Account operations
    int createAccount(int customer_id, const std::string &type);
    Account getAccount(int account_id);
    std::vector<Account> listAccountsByCustomer(int customer_id);

    // Transactions
    bool deposit(int account_id, double amount);
    bool withdraw(int account_id, double amount);
    bool transfer(int from_account_id, int to_account_id, double amount);
    std::vector<TransactionRecord> recentTransactions(int account_id, int limit=10);

private:
    // opaque pointer to hide MySQL connector headers from this header
    struct Impl;
    Impl* impl;
};

#endif // BANK_H

// -------------------- src/bank.cpp --------------------

#include "../include/bank.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <sstream>

// Include MySQL Connector/C++ headers
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

struct Bank::Impl {
    sql::mysql::MySQL_Driver *driver;
    std::unique_ptr<sql::Connection> conn;
    Impl(const std::string &host, const std::string &user, const std::string &pass, const std::string &db) {
        driver = sql::mysql::get_mysql_driver_instance();
        conn.reset(driver->connect(host, user, pass));
        conn->setSchema(db);
    }
};

Bank::Bank(const std::string &host, const std::string &user, const std::string &pass, const std::string &db) {
    try {
        impl = new Impl(host, user, pass, db);
    } catch (sql::SQLException &e) {
        std::cerr << "[DB Error] " << e.what() << std::endl;
        throw;
    }
}

Bank::~Bank() {
    delete impl;
}

int Bank::createCustomer(const std::string &name, const std::string &email, const std::string &phone) {
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("INSERT INTO customers(name,email,phone) VALUES(?,?,?)"));
        ps->setString(1, name);
        ps->setString(2, email);
        ps->setString(3, phone);
        ps->execute();
        std::unique_ptr<sql::Statement> st(impl->conn->createStatement());
        std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID() as id"));
        rs->next();
        return rs->getInt("id");
    } catch (sql::SQLException &e) {
        std::cerr << "[createCustomer error] " << e.what() << std::endl;
        return -1;
    }
}

std::vector<Customer> Bank::listCustomers() {
    std::vector<Customer> out;
    try {
        std::unique_ptr<sql::Statement> st(impl->conn->createStatement());
        std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT customer_id,name,email,phone FROM customers"));
        while (rs->next()) {
            Customer c;
            c.id = rs->getInt("customer_id");
            c.name = rs->getString("name");
            c.email = rs->getString("email");
            c.phone = rs->getString("phone");
            out.push_back(c);
        }
    } catch (sql::SQLException &e) {
        std::cerr << "[listCustomers error] " << e.what() << std::endl;
    }
    return out;
}

Customer Bank::getCustomer(int customer_id) {
    Customer c; c.id = -1;
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("SELECT customer_id,name,email,phone FROM customers WHERE customer_id = ?"));
        ps->setInt(1, customer_id);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
        if (rs->next()) {
            c.id = rs->getInt("customer_id");
            c.name = rs->getString("name");
            c.email = rs->getString("email");
            c.phone = rs->getString("phone");
        }
    } catch (sql::SQLException &e) {
        std::cerr << "[getCustomer error] " << e.what() << std::endl;
    }
    return c;
}

int Bank::createAccount(int customer_id, const std::string &type) {
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("INSERT INTO accounts(customer_id,account_type,balance) VALUES(?,?,0.0)"));
        ps->setInt(1, customer_id);
        ps->setString(2, type);
        ps->execute();
        std::unique_ptr<sql::Statement> st(impl->conn->createStatement());
        std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID() as id"));
        rs->next();
        return rs->getInt("id");
    } catch (sql::SQLException &e) {
        std::cerr << "[createAccount error] " << e.what() << std::endl;
        return -1;
    }
}

Account Bank::getAccount(int account_id) {
    Account a; a.id = -1;
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("SELECT account_id,customer_id,account_type,balance FROM accounts WHERE account_id = ?"));
        ps->setInt(1, account_id);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
        if (rs->next()) {
            a.id = rs->getInt("account_id");
            a.customer_id = rs->getInt("customer_id");
            a.type = rs->getString("account_type");
            a.balance = rs->getDouble("balance");
        }
    } catch (sql::SQLException &e) {
        std::cerr << "[getAccount error] " << e.what() << std::endl;
    }
    return a;
}

std::vector<Account> Bank::listAccountsByCustomer(int customer_id) {
    std::vector<Account> out;
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("SELECT account_id,customer_id,account_type,balance FROM accounts WHERE customer_id = ?"));
        ps->setInt(1, customer_id);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
        while (rs->next()) {
            Account a;
            a.id = rs->getInt("account_id");
            a.customer_id = rs->getInt("customer_id");
            a.type = rs->getString("account_type");
            a.balance = rs->getDouble("balance");
            out.push_back(a);
        }
    } catch (sql::SQLException &e) {
        std::cerr << "[listAccountsByCustomer error] " << e.what() << std::endl;
    }
    return out;
}

bool Bank::deposit(int account_id, double amount) {
    if (amount <= 0) return false;
    try {
        impl->conn->setAutoCommit(false);
        std::unique_ptr<sql::PreparedStatement> ps1(impl->conn->prepareStatement("UPDATE accounts SET balance = balance + ? WHERE account_id = ?"));
        ps1->setDouble(1, amount);
        ps1->setInt(2, account_id);
        ps1->execute();

        std::unique_ptr<sql::PreparedStatement> ps2(impl->conn->prepareStatement(
            "INSERT INTO transactions(account_id,type,amount,details) VALUES(?,?,?,?)"));
        ps2->setInt(1, account_id);
        ps2->setString(2, "DEPOSIT");
        ps2->setDouble(3, amount);
        ps2->setString(4, "Deposit via app");
        ps2->execute();

        impl->conn->commit();
        impl->conn->setAutoCommit(true);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "[deposit error] " << e.what() << std::endl;
        try { impl->conn->rollback(); impl->conn->setAutoCommit(true); } catch(...){}
        return false;
    }
}

bool Bank::withdraw(int account_id, double amount) {
    if (amount <= 0) return false;
    try {
        // check balance
        Account a = getAccount(account_id);
        if (a.id < 0) return false;
        if (a.balance < amount) return false;

        impl->conn->setAutoCommit(false);
        std::unique_ptr<sql::PreparedStatement> ps1(impl->conn->prepareStatement("UPDATE accounts SET balance = balance - ? WHERE account_id = ?"));
        ps1->setDouble(1, amount);
        ps1->setInt(2, account_id);
        ps1->execute();

        std::unique_ptr<sql::PreparedStatement> ps2(impl->conn->prepareStatement(
            "INSERT INTO transactions(account_id,type,amount,details) VALUES(?,?,?,?)"));
        ps2->setInt(1, account_id);
        ps2->setString(2, "WITHDRAW");
        ps2->setDouble(3, amount);
        ps2->setString(4, "Withdrawal via app");
        ps2->execute();

        impl->conn->commit();
        impl->conn->setAutoCommit(true);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "[withdraw error] " << e.what() << std::endl;
        try { impl->conn->rollback(); impl->conn->setAutoCommit(true); } catch(...){}
        return false;
    }
}

bool Bank::transfer(int from_account_id, int to_account_id, double amount) {
    if (amount <= 0) return false;
    try {
        Account a_from = getAccount(from_account_id);
        Account a_to = getAccount(to_account_id);
        if (a_from.id < 0 || a_to.id < 0) return false;
        if (a_from.balance < amount) return false;

        impl->conn->setAutoCommit(false);
        std::unique_ptr<sql::PreparedStatement> ps1(impl->conn->prepareStatement("UPDATE accounts SET balance = balance - ? WHERE account_id = ?"));
        ps1->setDouble(1, amount);
        ps1->setInt(2, from_account_id);
        ps1->execute();

        std::unique_ptr<sql::PreparedStatement> ps2(impl->conn->prepareStatement("UPDATE accounts SET balance = balance + ? WHERE account_id = ?"));
        ps2->setDouble(1, amount);
        ps2->setInt(2, to_account_id);
        ps2->execute();

        std::unique_ptr<sql::PreparedStatement> ps3(impl->conn->prepareStatement(
            "INSERT INTO transactions(account_id,type,amount,details) VALUES(?,?,?,?)"));
        ps3->setInt(1, from_account_id);
        ps3->setString(2, "TRANSFER");
        ps3->setDouble(3, amount);
        std::ostringstream det;
        det << "Transfer to account " << to_account_id;
        ps3->setString(4, det.str());
        ps3->execute();

        std::unique_ptr<sql::PreparedStatement> ps4(impl->conn->prepareStatement(
            "INSERT INTO transactions(account_id,type,amount,details) VALUES(?,?,?,?)"));
        ps4->setInt(1, to_account_id);
        ps4->setString(2, "DEPOSIT");
        ps4->setDouble(3, amount);
        std::ostringstream det2;
        det2 << "Transfer from account " << from_account_id;
        ps4->setString(4, det2.str());
        ps4->execute();

        impl->conn->commit();
        impl->conn->setAutoCommit(true);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "[transfer error] " << e.what() << std::endl;
        try { impl->conn->rollback(); impl->conn->setAutoCommit(true); } catch(...){}
        return false;
    }
}

std::vector<TransactionRecord> Bank::recentTransactions(int account_id, int limit) {
    std::vector<TransactionRecord> out;
    try {
        std::unique_ptr<sql::PreparedStatement> ps(
            impl->conn->prepareStatement("SELECT transaction_id,account_id,type,amount,details,created_at FROM transactions WHERE account_id = ? ORDER BY created_at DESC LIMIT ?"));
        ps->setInt(1, account_id);
        ps->setInt(2, limit);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
        while (rs->next()) {
            TransactionRecord t;
            t.id = rs->getInt("transaction_id");
            t.account_id = rs->getInt("account_id");
            t.type = rs->getString("type");
            t.amount = rs->getDouble("amount");
            t.details = rs->getString("details");
            t.created_at = rs->getString("created_at");
            out.push_back(t);
        }
    } catch (sql::SQLException &e) {
        std::cerr << "[recentTransactions error] " << e.what() << std::endl;
    }
    return out;
}

// -------------------- src/main.cpp --------------------

#include <iostream>
#include <limits>
#include "../include/bank.h"

void pause() {
    std::cout << "Press Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main() {
    // Update these DB credentials before running
    const std::string DB_HOST = "tcp://127.0.0.1:3306"; // or "tcp://localhost:3306"
    const std::string DB_USER = "root";
    const std::string DB_PASS = "yourpassword";
    const std::string DB_NAME = "banking_system";

    try {
        Bank bank(DB_HOST, DB_USER, DB_PASS, DB_NAME);
        while (true) {
            std::cout << "--- Simple Banking System ---\n";
            std::cout << "1. Create Customer\n2. List Customers\n3. Create Account\n4. List Accounts (by customer)\n";
            std::cout << "5. Deposit\n6. Withdraw\n7. Transfer\n8. View Account & Recent Transactions\n9. Exit\nChoose: ";
            int choice; if (!(std::cin >> choice)) break;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (choice == 1) {
                std::string name,email,phone;
                std::cout << "Name: "; std::getline(std::cin, name);
                std::cout << "Email: "; std::getline(std::cin, email);
                std::cout << "Phone: "; std::getline(std::cin, phone);
                int cid = bank.createCustomer(name,email,phone);
                if (cid > 0) std::cout << "Customer created with ID: " << cid << "\n";
                else std::cout << "Failed to create customer\n";
                pause();
            } else if (choice == 2) {
                auto customers = bank.listCustomers();
                for (auto &c: customers) {
                    std::cout << c.id << ": " << c.name << " (" << c.email << ") " << c.phone << "\n";
                }
                pause();
            } else if (choice == 3) {
                int cid; std::string type;
                std::cout << "Customer ID: "; std::cin >> cid; std::cin.ignore();
                std::cout << "Account type (SAVINGS/CURRENT): "; std::getline(std::cin, type);
                int aid = bank.createAccount(cid, type);
                if (aid > 0) std::cout << "Account created with ID: " << aid << "\n";
                else std::cout << "Failed to create account\n";
                pause();
            } else if (choice == 4) {
                int cid; std::cout << "Customer ID: "; std::cin >> cid; std::cin.ignore();
                auto accs = bank.listAccountsByCustomer(cid);
                for (auto &a: accs) {
                    std::cout << a.id << ": " << a.type << " Balance: " << a.balance << "\n";
                }
                pause();
            } else if (choice == 5) {
                int aid; double amt;
                std::cout << "Account ID: "; std::cin >> aid;
                std::cout << "Amount: "; std::cin >> amt;
                if (bank.deposit(aid, amt)) std::cout << "Deposit successful\n"; else std::cout << "Deposit failed\n";
                pause();
            } else if (choice == 6) {
                int aid; double amt;
                std::cout << "Account ID: "; std::cin >> aid;
                std::cout << "Amount: "; std::cin >> amt;
                if (bank.withdraw(aid, amt)) std::cout << "Withdrawal successful\n"; else std::cout << "Withdrawal failed (insufficient funds?)\n";
                pause();
            } else if (choice == 7) {
                int from,to; double amt;
                std::cout << "From Account ID: "; std::cin >> from;
                std::cout << "To Account ID: "; std::cin >> to;
                std::cout << "Amount: "; std::cin >> amt;
                if (bank.transfer(from, to, amt)) std::cout << "Transfer successful\n"; else std::cout << "Transfer failed\n";
                pause();
            } else if (choice == 8) {
                int aid; std::cout << "Account ID: "; std::cin >> aid; std::cin.ignore();
                Account a = bank.getAccount(aid);
                if (a.id > 0) {
                    std::cout << "Account " << a.id << " (" << a.type << ") Balance: " << a.balance << "\n";
                    auto tx = bank.recentTransactions(aid, 10);
                    for (auto &t : tx) {
                        std::cout << t.created_at << " | " << t.type << " | " << t.amount << " | " << t.details << "\n";
                    }
                } else {
                    std::cout << "Account not found\n";
                }
                pause();
            } else break;
        }
    } catch (std::exception &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
    }
    return 0;
}
