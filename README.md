# Banking System in C

A simple banking system implementation in C that allows account management, transactions, and admin functionality,file creations.

## Features

- Account creation with secure PIN
- Balance inquiry
- Deposit and withdrawal
- PIN change
- Account deletion
- Admin panel for viewing all accounts

## Prerequisites

- C compiler (gcc, clang, or MSVC)
- (Optional) OpenSSL for secure hashing (uncomment `USE_OPENSSL_HASH` in the code)

## Compilation

```bash
gcc trans.c.c -o banking_system
# With OpenSSL support:
# gcc trans.c.c -o banking_system -lssl -lcrypto -DUSE_OPENSSL_HASH
```

## Usage

Run the compiled program and follow the on-screen menu:

```
./banking_system
```

## Admin Access

Default admin credentials:
- Username: admin
- Password: 90253

## Security Note

For production use, please:
1. Change the default admin credentials
2. Enable OpenSSL hashing by uncommenting `#define USE_OPENSSL_HASH`
3. Never commit sensitive data to version control
