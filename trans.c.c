#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#endif

// Uncomment next line and link with -lcrypto to use real SHA-256
// #define USE_OPENSSL_HASH

#ifdef USE_OPENSSL_HASH
#include <openssl/sha.h>
#endif

#define FILENAME "accounts.dat"
#define TEMPFILE  "temp_accounts.dat"

typedef struct {
    int account_number;
    char name[50];
    char pin_hash[65]; // 64 hex chars + null
    double balance;
} Account;

// Helper hash function (SHA-256 in hex)
void hash_pin(const char *pin, char pin_hash[65]) {
#ifdef USE_OPENSSL_HASH
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)pin, strlen(pin), hash);
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(pin_hash + (i*2), "%02x", hash[i]);
    pin_hash[64] = '\0';
#else
    // Fallback dummy hash: NOT SAFE, replace with OpenSSL in real use!
    unsigned int dummy = 0;
    for (const char *p = pin; *p; ++p) dummy = dummy * 33 ^ (unsigned char)*p;
    snprintf(pin_hash, 65, "%08x%08x%08x%08x", dummy, ~dummy, dummy ^ 0xabcdef, (~dummy) ^ 0x123456);
#endif
}

// Get hidden PIN/password (works on Linux; fallback to plain for Windows/others)
void get_hidden_input(char *buf, size_t sz) {
#ifdef _WIN32
    // Poor man's hidden input (not robust)
    int c, i=0;
    while (i < sz-1 && (c = _getch()) && c != '\r' && c != '\n') {
        if (c == 8 && i > 0) { printf("\b \b"); --i; continue; }
        buf[i++] = c;
        printf("*");
    }
    buf[i] = 0;
    puts("");
#else
    char *pwd = getpass("");
    strncpy(buf, pwd, sz-1);
    buf[sz-1] = 0;
#endif
}

int generate_account_number() {
    int last_num = 100000;
    FILE *fp = fopen(FILENAME, "rb");
    Account acc;
    if (fp) {
        while (fread(&acc, sizeof(Account), 1, fp))
            if (acc.account_number > last_num) last_num = acc.account_number;
        fclose(fp);
    }
    return last_num + 1;
}

void get_clean_line(char *buf, size_t sz) {
    if (fgets(buf, sz, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
        else while (getchar()!='\n' && !feof(stdin)); // flush
    }
}

int generate_pin() {
    return 1000 + rand() % 9000;
}

void save_account(const Account *acc) {
    FILE *fp = fopen(FILENAME, "ab");
    if (!fp) { perror("File error"); return; }
    fwrite(acc, sizeof(Account), 1, fp);
    fclose(fp);
}

// Update is now atomic: uses a tempfile
void update_account(const Account *acc) {
    FILE *fp = fopen(FILENAME, "rb");
    FILE *ft = fopen(TEMPFILE, "wb");
    if (!fp || !ft) { perror("Atomic file error"); if(fp) fclose(fp); if(ft) fclose(ft); return;}
    Account tmp;
    while (fread(&tmp, sizeof(Account), 1, fp)) {
        if (tmp.account_number == acc->account_number)
            fwrite(acc, sizeof(Account), 1, ft);
        else
            fwrite(&tmp, sizeof(Account), 1, ft);
    }
    fclose(fp); fclose(ft);
    remove(FILENAME); rename(TEMPFILE, FILENAME);
}

// Create account, PIN is hashed, print only once on create
void create_account() {
    Account acc = {0};
    acc.account_number = generate_account_number();
    printf("Enter your name: ");
    get_clean_line(acc.name, sizeof(acc.name));
    int pin_num = generate_pin();
    char pin_str[10];
    snprintf(pin_str, 10, "%04d", pin_num);
    hash_pin(pin_str, acc.pin_hash);
    acc.balance = 0.0;
    save_account(&acc);
    puts("\nAccount created successfully!");
    printf("Account Number: %d\nPIN: %04d\nPlease remember your credentials.\n", acc.account_number, pin_num);
}

// Login (returns 1 if success and fills acc_out)
int login(Account *acc_out) {
    int acc_num, tries = 3;
    char pin_input[10], pin_hash[65];
    printf("\nAccount number: ");
    if (scanf("%d", &acc_num) != 1) { puts("Invalid number."); while(getchar()!='\n'); return 0; }
    while(getchar()!='\n'); // flush

    printf("PIN: ");
    get_hidden_input(pin_input, 10);
    hash_pin(pin_input, pin_hash);

    FILE *fp = fopen(FILENAME, "rb");
    if (!fp) { puts("File error"); return 0; }
    Account acc;
    while (fread(&acc, sizeof(Account), 1, fp)) {
        if (acc.account_number == acc_num) {
            fclose(fp);
            if (strcmp(acc.pin_hash, pin_hash)==0) {
                *acc_out = acc;
                return 1;
            } else {
                puts("Incorrect PIN!");
                return 0;
            }
        }
    }
    puts("Account not found!");
    fclose(fp);
    return 0;
}

// View balance
void view_balance() {
    Account acc;
    if (login(&acc)) {
        printf("\nWelcome, %s!\nAccount: %d\nBalance: Rs. %.2lf\n",
               acc.name, acc.account_number, acc.balance);
    }
}

void deposit() {
    Account acc;
    if (!login(&acc)) return;
    double amt = 0.0;
    printf("Amount to deposit: Rs. ");
    while (scanf("%lf", &amt)!=1 || amt <= 0) {
        puts("Invalid amount. Try again:");
        while(getchar()!='\n');
    }
    acc.balance += amt;
    update_account(&acc);
    printf("Deposit successful. New balance: Rs. %.2lf\n", acc.balance);
}

void withdraw() {
    Account acc;
    if (!login(&acc)) return;
    double amt = 0.0;
    printf("Amount to withdraw: Rs. ");
    while (scanf("%lf", &amt)!=1 || amt <= 0 || amt > acc.balance) {
        if (amt > acc.balance)
            puts("Insufficient funds. Try smaller amount.");
        else
            puts("Invalid amount. Try again:");
        while(getchar()!='\n');
    }
    acc.balance -= amt;
    update_account(&acc);
    printf("Withdraw successful. New balance: Rs. %.2lf\n", acc.balance);
}

void change_pin() {
    Account acc;
    if (!login(&acc)) return;
    char pin_input[10], old_hash[65], newpin[10], confirm[10];
    printf("Enter current PIN to confirm: ");
    get_hidden_input(pin_input, 10);
    hash_pin(pin_input, old_hash);
    if (strcmp(old_hash, acc.pin_hash) != 0) {
        puts("PIN mismatch.");
        return;
    }
    while (1) {
        printf("Enter new 4-digit PIN: ");
        get_hidden_input(newpin, 10);
        if (strlen(newpin) != 4 || strspn(newpin,"0123456789") != 4) {
            puts("PIN must be 4 digits.");
            continue;
        }
        printf("Re-enter new PIN to confirm: ");
        get_hidden_input(confirm, 10);
        if (strcmp(newpin, confirm)!=0) {
            puts("PINs don't match. Try again.");
            continue;
        }
        hash_pin(newpin, acc.pin_hash);
        update_account(&acc);
        puts("PIN changed successfully.");
        break;
    }
}

// Delete account securely
void delete_account() {
    Account acc;
    if (!login(&acc)) return;
    char sure[10];
    printf("Are you SURE you want to delete account %d? Type YES to confirm: ", acc.account_number);
    get_clean_line(sure, 10);
    if (strcmp(sure, "YES") != 0) {
        puts("Deletion cancelled.");
        return;
    }
    FILE *fp = fopen(FILENAME, "rb");
    FILE *ft = fopen(TEMPFILE, "wb");
    if (!fp || !ft) { puts("File error!"); if(fp) fclose(fp); if(ft) fclose(ft); return;}
    Account temp;
    int found = 0;
    while (fread(&temp, sizeof(Account), 1, fp)) {
        if (temp.account_number == acc.account_number)
            found = 1;
        else
            fwrite(&temp, sizeof(Account), 1, ft);
    }
    fclose(fp); fclose(ft);
    if (found) {
        remove(FILENAME); rename(TEMPFILE, FILENAME);
        printf("Account %d deleted successfully.\n", acc.account_number);
    } else {
        remove(TEMPFILE);
        puts("Failed to delete (account not found).");
    }
}

// Admin details view (one session per run)
void details_admin_session() {
    FILE *fp = fopen(FILENAME, "rb");
    if (!fp) { puts("No accounts found."); return; }
    Account acc;
    printf("\n---- All stored accounts ----\n");
    while (fread(&acc, sizeof(Account), 1, fp)) {
        printf("Acc: %d, Name: %s, Bal: %.2lf\n",
               acc.account_number, acc.name, acc.balance);
    }
    puts("-------------------------");
    fclose(fp);
}

void admin_details() {
    static int admin_logged_in = 0;
    static char username[32] = "", password[32] = "";
    if (!admin_logged_in) {
        printf("--- Admin Login Required ---\nUsername: ");
        get_clean_line(username, 32);
        printf("Password: ");
        get_hidden_input(password, 32);
        if (strcmp(username,"admin")!=0 || strcmp(password,"90253")!=0) {
            puts("Access denied. Invalid admin credentials.");
            return;
        }
        admin_logged_in = 1;
    }
    details_admin_session();
}

int main() {
    srand((unsigned) time(NULL));
    int choice;
    while (1) {
        printf("\n==== Bank Transaction Menu ====\n");
        printf("1. Create Account\n");
        printf("2. View Balance\n");
        printf("3. Deposit\n");
        printf("4. Withdraw\n");
        printf("5. Change PIN\n");
        printf("6. Delete Account\n");
        printf("7. Details (admin only)\n");
        printf("8. Exit\n");
        printf("Choose an option: ");
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); break; }
        while(getchar()!='\n'); // clear buffer

        switch (choice) {
            case 1: create_account(); break;
            case 2: view_balance();   break;
            case 3: deposit();        break;
            case 4: withdraw();       break;
            case 5: change_pin();     break;
            case 6: delete_account(); break;
            case 7: admin_details();  break;
            case 8: puts("Thank you."); exit(0);
            default: puts("Invalid option.");
        }
    }
    return 0;
}