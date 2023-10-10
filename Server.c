#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "headers.h"
#include <string.h>

void unlock(int fd, struct flock lock){
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
}
void setLockCust(int fd_custs, struct flock lock_cust){

    lock_cust.l_len = 0;
    lock_cust.l_type = F_RDLCK;
    lock_cust.l_start = 0;
    lock_cust.l_whence = SEEK_SET;
    fcntl(fd_custs, F_SETLKW, &lock_cust);

    return ;
}


void productWriteLock(int fd, struct flock lock){
    lseek(fd, (-1)*sizeof(struct product), SEEK_CUR);
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_CUR;
    lock.l_start = 0;
    lock.l_len = sizeof(struct product);

    fcntl(fd, F_SETLKW, &lock);
}
void productReadLock(int fd, struct flock lock){
    lock.l_len = 0;
    lock.l_type = F_RDLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    fcntl(fd, F_SETLKW, &lock);
}


int getOffset(int cust_id, int fd_custs){
    if (cust_id < 0){
        return -1;
    }
    struct flock lock_cust;
    setLockCust(fd_custs, lock_cust);
    struct index id;

    while (read(fd_custs, &id, sizeof(struct index))){
        if (id.custid == cust_id){
            unlock(fd_custs, lock_cust);
            return id.offset;
        }
    }
    unlock(fd_custs, lock_cust);
    return -1;
}
void cartOffsetLock(int fd_cart, struct flock lock_cart, int offset, int ch){
    lock_cart.l_whence = SEEK_SET;
    lock_cart.l_len = sizeof(struct cart);
    lock_cart.l_start = offset;
    if (ch == 1){
        //rdlck
        lock_cart.l_type = F_RDLCK;
    }else{
        //wrlck
        lock_cart.l_type = F_WRLCK;
    }
    fcntl(fd_cart, F_SETLKW, &lock_cart);
    lseek(fd_cart, offset, SEEK_SET);
}

void listProducts(int fd, int new_fd){

    struct flock lock;
    productReadLock(fd, lock);

    struct product p;
    while (read(fd, &p, sizeof(struct product))){
        if (p.id != -1){
            write(new_fd, &p, sizeof(struct product));
        }
    }
    
    p.id = -1;
    write(new_fd, &p, sizeof(struct product));
    unlock(fd, lock);
}
void addProducts(int fd, int new_fd, int fd_admin){

    char name[50];
    char response[100];
    int id, qty, price;

    struct product p1;
    int n = read(new_fd, &p1, sizeof(struct product));

    strcpy(name, p1.name);
    id = p1.id;
    qty = p1.qty;
    price = p1.price;

    struct flock lock;
    productReadLock(fd, lock);

    struct product p;

    int flg = 0;
    while (read(fd, &p, sizeof(struct product))){

        if (p.qty > 0 && p.id == id){
            write(new_fd, "Duplicate product\n", sizeof("Duplicate product\n"));
            sprintf(response, "Adding product with product id %d failed as product id is duplicate\n", id);
            write(fd_admin, response, strlen(response));
            unlock(fd, lock);
            flg = 1;
            return;
        }
    }

    if (!flg){

        lseek(fd, 0, SEEK_END);
        p.id = id;
        strcpy(p.name, name);
        p.price = price;
        p.qty = qty;

        write(fd, &p, sizeof(struct product));
        write(new_fd, "Added successfully\n", sizeof("Added succesfully\n"));
        sprintf(response, "New product with product id %d added successfully\n", id);
        write(fd_admin, response, strlen(response));
        unlock(fd, lock);   
    }
}

void updateProduct(int fd, int new_fd, int ch, int fd_admin){
    int id;
    int val = -1;
    struct product p1;
    read(new_fd, &p1, sizeof(struct product));
    id = p1.id;

    char response[100];
    
    if (ch == 1){
        val = p1.price;
    }else{
        val = p1.qty;
    }

    struct flock lock;
    productReadLock(fd, lock);

    int flg = 0;
    
    struct product p;
    while (read(fd, &p, sizeof(struct product))){
        if (p.id == id){

            unlock(fd, lock);
            productWriteLock(fd, lock);
            int old;
            if (ch == 1){
                old = p.price;
                p.price = val;
            }else{
                old = p.qty;
                p.qty = val;
            }

            write(fd, &p, sizeof(struct product));
            if (ch == 1){
                write(new_fd, "Price modified successfully", sizeof("Price modified successfully"));
                sprintf(response, "Price of product with product id %d modified from %d to %d \n", id, old, val);
                write(fd_admin, response, strlen(response));
            }else{
                sprintf(response, "Quantity of product with product id %d modified from %d to %d \n", id, old, val);
                write(fd_admin, response, strlen(response));
                write(new_fd, "Quantity modified successfully", sizeof("Quantity modified successfully"));               
            }

            unlock(fd, lock);
            flg = 1;
            break;
        }
    }

    if (!flg){
        write(new_fd, "Product id invalid", sizeof("Product id invalid"));
        unlock(fd, lock);
    }
}

void deleteProduct(int fd, int new_fd, int id, int fd_admin){

    struct flock lock;
    productReadLock(fd, lock);
    char response[100];

    struct product p;
    int flg = 0;
    while (read(fd, &p, sizeof(struct product))){
        if (p.id == id){
            
            unlock(fd, lock);
            productWriteLock(fd, lock);

            p.id = -1;
            strcpy(p.name, "");
            p.price = -1;
            p.qty = -1;

            write(fd, &p, sizeof(struct product));
            write(new_fd, "Delete successful", sizeof("Delete successful"));
            sprintf(response, "Product with product id %d deleted succesfully\n", id);
            write(fd_admin, response, strlen(response));

            unlock(fd, lock);
            flg = 1;
            return;
        }
    }

    if (!flg){
        sprintf(response, "Deleting product with product id %d failed as product does not exist\n", id);
        write(fd_admin, response, strlen(response));
        write(new_fd, "Product id invalid", sizeof("Product id invalid"));
        unlock(fd, lock);
    }
}

void viewCart(int fd_cart, int new_fd, int fd_custs){
    int cust_id = -1;
    read(new_fd, &cust_id, sizeof(int));

    int offset = getOffset(cust_id, fd_custs);    
    struct cart c;

    if (offset == -1){

        struct cart c;
        c.custid = -1;
        write(new_fd, &c, sizeof(struct cart));
        
    }else{
        struct cart c;
        struct flock lock_cart;
        
        cartOffsetLock(fd_cart, lock_cart, offset, 1);
        read(fd_cart, &c, sizeof(struct cart));
        write(new_fd, &c, sizeof(struct cart));
        unlock(fd_cart, lock_cart);
    }
}

void addCustomer(int fd_cart, int fd_custs, int new_fd){
    char buf;
    read(new_fd, &buf, sizeof(char));
    if (buf == 'y'){

        struct flock lock;
        setLockCust(fd_custs, lock);
        
        int max_id = -1; 
        struct index id ;
        while (read(fd_custs, &id, sizeof(struct index))){
            if (id.custid > max_id){
                max_id = id.custid;
            }
        }

        max_id ++;
        
        id.custid = max_id;
        id.offset = lseek(fd_cart, 0, SEEK_END);
        lseek(fd_custs, 0, SEEK_END);
        write(fd_custs, &id, sizeof(struct index));

        struct cart c;
        c.custid = max_id;
        int i=0;
        while(i<MAX_PROD){
            c.products[i].id = -1;
            strcpy(c.products[i].name , "");
            c.products[i].qty = -1;
            c.products[i].price = -1;
            i=i+1;
        }

        write(fd_cart, &c, sizeof(struct cart));
        unlock(fd_custs, lock);
        write(new_fd, &max_id, sizeof(int));
    }
}


void editProductInCart(int fd, int fd_cart, int fd_custs, int new_fd){

    int cust_id = -1;
    read(new_fd, &cust_id, sizeof(int));

    int offset = getOffset(cust_id, fd_custs);

    write(new_fd, &offset, sizeof(int));
    if (offset == -1){
        return;
    }

    struct flock lock_cart;
    cartOffsetLock(fd_cart, lock_cart, offset, 1);
    struct cart c;
    read(fd_cart, &c, sizeof(struct cart));

    int pid, qty;
    struct product p;
    read(new_fd, &p, sizeof(struct product));

    pid = p.id;
    qty = p.qty;

    int flg;
    flg = 0;
    int i=0;
    while(i<MAX_PROD){
        if (c.products[i].id == pid){
            
            struct flock lock_prod;
            productReadLock(fd, lock_prod);

            struct product p1;
            while (read(fd, &p1, sizeof(struct product))){
                if (p1.id == pid && p1.qty > 0) {
                    if (p1.qty >= qty){
                        flg = 1;
                        break;
                    }else{
                        flg = 0;
                        break;
                    }
                }
            }

            unlock(fd, lock_prod);
            break;
        }
        i=i+1;
    }
    unlock(fd_cart, lock_cart);

    if (!flg){
        write(new_fd, "Product id not in the order or out of stock\n", sizeof("Product id not in the order or out of stock\n"));
        return;
    }

    c.products[i].qty = qty;
    write(new_fd, "Update successful\n", sizeof("Update successful\n"));
    cartOffsetLock(fd_cart, lock_cart, offset, 2);
    write(fd_cart, &c, sizeof(struct cart));
    unlock(fd_cart, lock_cart);
}

void addProductToCart(int fd, int fd_cart, int fd_custs, int new_fd){
    int cust_id;
    cust_id = -1;
    read(new_fd, &cust_id, sizeof(int));
    int offset = getOffset(cust_id, fd_custs);

    write(new_fd, &offset, sizeof(int));

    if (offset == -1){
        return;
    }

    struct flock lock_cart;
    
    int i = -1;
    cartOffsetLock(fd_cart, lock_cart, offset, 1);
    struct cart c;
    read(fd_cart, &c, sizeof(struct cart));

    struct flock lock_prod;
    productReadLock(fd, lock_prod);
    
    struct product p;
    read(new_fd, &p, sizeof(struct product));

    int prod_id = p.id;
    int qty = p.qty;

    struct product p1;
    int found = 0;
    while (read(fd, &p1, sizeof(struct product))){
        if (p1.id == p.id) {
            if (p1.qty >= p.qty){
                // p1.qty -= p.qty;
                found = 1;
                break;
            }
        }
    }
    unlock(fd_cart, lock_cart);
    unlock(fd, lock_prod);

    if (!found){
        write(new_fd, "Product id invalid or out of stock\n", sizeof("Product id invalid or out of stock\n"));
        return;
    }

    int flg = 0;
    
    int flg1 = 0;
    int l=0;
    while(l<MAX_PROD){
        if (c.products[l].id == p.id){
            flg1 = 1;
            break;
        }
        l++;
    }

    if (flg1){
        write(new_fd, "Product already exists in cart\n", sizeof("Product already exists in cart\n"));
        return;
    }
    int j=0;
    while(j<MAX_PROD){
        if (c.products[j].id == -1){
            flg = 1;
            c.products[j].id = p.id;
            c.products[j].qty = p.qty;
            strcpy(c.products[j].name, p1.name);
            c.products[j].price = p1.price;
            break;

        }else if (c.products[j].qty <= 0){
            flg = 1;
            c.products[j].id = p.id;
            c.products[j].qty = p.qty;
            strcpy(c.products[j].name, p1.name);
            c.products[j].price = p1.price;
            break;

        }
        j++;
    }

    if (!flg){
        write(new_fd, "Cart limit reached\n", sizeof("Cart limit reached\n"));
        return;
    }

    write(new_fd, "Item added to cart\n", sizeof("Item added to cart\n"));

    cartOffsetLock(fd_cart, lock_cart, offset, 2);    
    write(fd_cart, &c, sizeof(struct cart));
    unlock(fd_cart, lock_cart);

}



void payment(int fd, int fd_cart, int fd_custs, int new_fd){
    int cust_id = -1;
    read(new_fd, &cust_id, sizeof(int));

    int offset;
    offset = getOffset(cust_id, fd_custs);

    write(new_fd, &offset, sizeof(int));
    if (offset == -1){
        return;
    }

    struct flock lock_cart;
    cartOffsetLock(fd_cart, lock_cart, offset, 1);

    struct cart c;
    read(fd_cart, &c, sizeof(struct cart));
    unlock(fd_cart, lock_cart);
    write(new_fd, &c, sizeof(struct cart));

    int total = 0;
    int i=0; 
    while(i<MAX_PROD){

        if (c.products[i].id != -1){
            write(new_fd, &c.products[i].qty, sizeof(int));

            struct flock lock_prod;
            productReadLock(fd, lock_prod);
            lseek(fd, 0, SEEK_SET);

            struct product p;
            int flg = 0;
            while (read(fd, &p, sizeof(struct product))){

                if (p.id == c.products[i].id && p.qty > 0) {
                    int min ;
                    if (c.products[i].qty >= p.qty){
                        min = p.qty;
                    }else{
                        min = c.products[i].qty;
                    }

                    flg =1;
                    write(new_fd, &min, sizeof(int));
                    write(new_fd, &p.price, sizeof(int));
                    break;
                }
            }

            unlock(fd, lock_prod);

            if (!flg){
                //product got deleted midway
                int val = 0;
                write(new_fd, &val, sizeof(int));
                write(new_fd, &val, sizeof(int));
            }
        }    
        i=i+1;  
    }

    char ch;
    read(new_fd, &ch, sizeof(char));
    for (int i=0; i<MAX_PROD; i++){

        struct flock lock_prod;
        productReadLock(fd, lock_prod);
        lseek(fd, 0, SEEK_SET);

        struct product p;
        while (read(fd, &p, sizeof(struct product))){

            if (p.id == c.products[i].id) {
                int min ;
                if (c.products[i].qty >= p.qty){
                    min = p.qty;
                }else{
                    min = c.products[i].qty;
                }
                unlock(fd, lock_prod);
                productWriteLock(fd, lock_prod);
                p.qty = p.qty - min;

                write(fd, &p, sizeof(struct product));
                unlock(fd, lock_prod);
            }
        }

        unlock(fd, lock_prod);
    }
    
    cartOffsetLock(fd_cart, lock_cart, offset, 2);

    for (int i=0; i<MAX_PROD; i++){
        c.products[i].id = -1;
        strcpy(c.products[i].name, "");
        c.products[i].price = -1;
        c.products[i].qty = -1;
    }

    write(fd_cart, &c, sizeof(struct cart));
    write(new_fd, &ch, sizeof(char));
    unlock(fd_cart, lock_cart);

    read(new_fd, &total, sizeof(int));
    read(new_fd, &c, sizeof(struct cart));

    int fd_rec = open("receipt.txt", O_CREAT | O_RDWR, 0777);
    write(fd_rec, "ProductID\tProductName\tQuantity\tPrice\n", strlen("ProductID\tProductName\tQuantity\tPrice\n"));
    char temp[100];
    int j=0;
    while(j<MAX_PROD){
        if (c.products[j].id != -1){
            sprintf(temp, "%d\t%s\t%d\t%d\n", c.products[j].id, c.products[j].name, c.products[j].qty, c.products[j].price);
            write(fd_rec, temp, strlen(temp));
        }
        j++;
    }
    sprintf(temp, "Total - %d\n", total);
    write(fd_rec, temp, strlen(temp));
    close(fd_rec);
}

void generateAdminReceipt(int fd_admin, int fd){
    struct flock lock;
    productReadLock(fd, lock);
    write(fd_admin, "Current Inventory:\n", strlen("Current Inventory:\n"));
    write(fd_admin, "ProductID\tProductName\tQuantity\tPrice\n", strlen("ProductID\tProductName\tQuantity\tPrice\n"));

    lseek(fd, 0, SEEK_SET);
    struct product p;
    while (read(fd, &p, sizeof(struct product))){
        if (p.id != -1){
            char temp[100];
            sprintf(temp, "%d\t%s\t%d\t%d\n",p.id, p.name, p.qty, p.price);
            write(fd_admin, temp, strlen(temp));
        }
    }
    unlock(fd, lock);
}

int main(){
    printf("Setting up the server\n");

    //file with all the records is called records.txt

    int fd = open("rec.txt", O_RDWR | O_CREAT, 0777);
    int fd_cart = open("Orderlist.txt", O_RDWR | O_CREAT, 0777);
    int fd_custs = open("cust.txt", O_RDWR | O_CREAT, 0777);
    int fd_admin = open("Receipt_Admin.txt", O_RDWR | O_CREAT, 0777);
    lseek(fd_admin, 0, SEEK_END);

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1){
        perror("Error: ");
        return -1;
    }

    struct sockaddr_in serv, cli;
    
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(5555);

    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("Error: ");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&serv, sizeof(serv)) == -1){
        perror("Error: ");
        return -1;
    }

    if (listen(sockfd, 5) == -1){
        perror("Error: ");
        return -1;
    }

    int size;
    size = sizeof(cli);
    printf("Server successfully set up \n");

    while (1){

        int new_fd = accept(sockfd, (struct sockaddr *)&cli, &size);
        if (new_fd == -1){
            
            return -1;
        }

        if (!fork()){
            printf("Connection successful with the client\n");
            close(sockfd);

            int user;
            read(new_fd, &user, sizeof(int));
            
            if (user == 1){

                char ch;
                while (1){
                    read(new_fd, &ch, sizeof(char));

                    lseek(fd, 0, SEEK_SET);
                    lseek(fd_cart, 0, SEEK_SET);
                    lseek(fd_custs, 0, SEEK_SET);

                    if (ch == 'a'){
                        close(new_fd);
                        break;
                    }
                    else if (ch == 'b'){
                        listProducts(fd, new_fd);
                    }

                    else if (ch == 'c'){
                        viewCart(fd_cart, new_fd, fd_custs);
                    }

                    else if (ch == 'd'){
                        addProductToCart(fd, fd_cart, fd_custs, new_fd);
                    }

                    else if (ch == 'e'){
                        editProductInCart(fd, fd_cart, fd_custs, new_fd);
                    }

                    else if (ch == 'f'){
                        payment(fd, fd_cart, fd_custs, new_fd);
                    }

                    else if (ch == 'g'){
                        addCustomer(fd_cart, fd_custs, new_fd);
                    }
                }
                printf("Connection is terminated\n");

            }
            else if (user == 2){
                char ch;
                while (1){
                    read(new_fd, &ch, sizeof(ch));

                    lseek(fd, 0, SEEK_SET);
                    lseek(fd_cart, 0, SEEK_SET);
                    lseek(fd_custs, 0, SEEK_SET);

                    if (ch == 'a'){
                        addProducts(fd, new_fd, fd_admin);
                    } 
                    else if (ch == 'b'){
                        int id;
                        read(new_fd, &id, sizeof(int));
                        deleteProduct(fd, new_fd, id, fd_admin);
                    }
                    else if (ch == 'c'){
                        updateProduct(fd, new_fd, 1, fd_admin);
                    }

                    else if (ch == 'd'){
                        updateProduct(fd, new_fd, 2, fd_admin);
                    }

                    else if (ch == 'e'){
                        listProducts(fd, new_fd);
                    }

                    else if (ch == 'f'){
                        close(new_fd);
                        generateAdminReceipt(fd_admin, fd);
                        break;
                    }
                    else{
                        continue;
                    }
                }
            }
            printf("Connection is terminated\n");

        }else{
            close(new_fd);
        }
    }
}