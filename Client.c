#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include "headers.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

void displayMenuAdmin(){
    printf("Menu \n");
    printf("a. To add a Product\n");
    printf("b. To delete any product\n");
    printf("c. To update price of an existing product\n");
    printf("d. To update quantity of an existing product\n");
    printf("e. To visit your inventory\n");
    printf("f. To Exit The Program\n");
    printf("Please enter your choice\n");
}
void displayMenuUser(){
    printf("Menu :\n");
    printf("a. To exit Menu\n");
    printf("b. To view all the products available\n");
    printf("c. To visit your cart\n");
    printf("d. To add products to your cart\n");
    printf("e. To edit an existing product in your cart\n");
    printf("f. Proceed for payment\n");
    printf("g. To Register a New Customer\n");
    printf("Please Enter Your Choice\n");
}


void printProduct(struct product p){
    if (p.qty > 0 && p.id != -1){
        printf("%d\t%s\t%d\t%d\n", p.id, p.name, p.qty, p.price);
    }
}

void getInventory(int sockfd){
    printf("Fetching the data\n");
    printf("ProductID\tProductName\tQuantityInStock\tPrice\n");
    while (1){
        struct product p;
        read(sockfd, &p, sizeof(struct product));
        if (p.id != -1){
            printProduct(p);
        }else{
            break;
        }
    }
}

void generateReceipt(int total, struct cart c, int sockfd){

    write(sockfd, &total, sizeof(int));
    write(sockfd, &c, sizeof(struct cart));
    
}

int calculateTotal(struct cart c){
    int total = 0,i=0;
    while(i<MAX_PROD){
        if (c.products[i].id != -1){
            total += c.products[i].qty * c.products[i].price;
        }
        i=i+1;
    }

    return total;
}

//input methods
int prodIdTaker(){
    int prodId ;
    prodId= -1;
    while (1){
        printf("Enter the product id\n");
        scanf("%d", &prodId);

        if (prodId < 0){
            printf("Product id cannot be negative, please try again\n");
        }else{
            break;
        }
    }
    return prodId;
}
int custIdTaker(){
    int custId = -1;
    while (1){
        printf("Enter the customer id\n");
        scanf("%d", &custId);

        if (custId < 0){
            printf("Customer id cannot be negative, please try again\n");
        }else{
            break;
        }
    }
    return custId;
}

int quantityTaker(){
    int qty = -1;
    while (1){
        printf("Enter the quantity\n");
        scanf("%d", &qty);

        if (qty < 0){
            printf("Quantity cannot be negative,please try again\n");
        }else{
            break;
        }

    }
    return qty;
}
int priceTaker(){
    int price;
    price = -1;
    while (1){
        printf("Enter the price\n");
        scanf("%d", &price);

        if (price < 0){
            printf("Price cannot be negative, please try again\n");
        }else{
            break;
        }
    }
    return price;
}


int main(){
    printf("Establishing connection with the server\n");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1){
        perror("Error: ");
        return -1;
    }

    struct sockaddr_in serv;
    
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(5555);

    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) == -1){
        perror("Error: ");
        return -1;
    }

    printf("Success\n");
    printf("Are you a user or the admin? Enter 1 for user, 2 for admin\n");
    int user;
    scanf("%d", &user);
    write(sockfd, &user, sizeof(user));

    if (user == 1){
        while (1){
            displayMenuUser();
            char ch;
            scanf("%c",&ch);
            scanf("%c",&ch);

            write(sockfd, &ch, sizeof(char));

            if (ch == 'a'){
                break;
            }
            else if (ch == 'b'){
                getInventory(sockfd);
            }
            else if (ch == 'c'){
                int cusid = custIdTaker();
                
                write(sockfd, &cusid, sizeof(int));

                struct cart o;
                read(sockfd, &o, sizeof(struct cart));

                if (o.custid != -1){
                    printf("Customer ID %d\n", o.custid);
                    printf("ProductID\tProductName\tQuantityInStock\tPrice\n");
                    int i=0;
                    while(i<MAX_PROD){
                        printProduct(o.products[i]);
                        i++;
                    }
                }else{
                    printf("Wrong customer id provided\n");
                }
            }

            else if (ch == 'd'){
                int cusid;
                cusid = custIdTaker();
                
                write(sockfd, &cusid, sizeof(int));

                int res;
                read(sockfd, &res, sizeof(int));
                if (res == -1){
                    printf("Invalid customer id\n");
                    continue;
                }
                char response[80];
                int pid;
                int qty;
                pid = prodIdTaker();
                
                while (1){
                    printf("Enter quantity\n");
                    scanf("%d", &qty);

                    if (qty <= 0){
                        printf("Quantity can't be <= 0, try again\n");
                    }else{
                        break;
                    }
                }

                struct product p;
                p.id = pid;
                p.qty = qty;

                write(sockfd, &p, sizeof(struct product));
                read(sockfd, response, sizeof(response));
                printf("%s", response);
                
            }

            else if (ch == 'e'){
                int cusid ;
                cusid= custIdTaker();
                
                write(sockfd, &cusid, sizeof(int));

                int res;
                read(sockfd, &res, sizeof(int));
                if (res == -1){
                    printf("Invalid customer id\n");
                    continue;
                }

                
                int pid = prodIdTaker();
                int qty = quantityTaker();
                
                struct product p;
                p.id = pid;
                p.qty = qty;

                write(sockfd, &p, sizeof(struct product));

                char response[80];
                read(sockfd, response, sizeof(response));
                printf("%s", response);
            }

            else if (ch == 'f'){
                int cusid = custIdTaker();
                write(sockfd, &cusid, sizeof(int));

                int res;
                read(sockfd, &res, sizeof(int));
                if (res == -1){
                    printf("Customer id is Invalid\n");
                    continue;
                }

                struct cart c;
                read(sockfd, &c, sizeof(struct cart));

                int ordered;
                int instock;
                int price, j=0;
                while(j<MAX_PROD){

                    if (c.products[j].id != -1){
                        read(sockfd, &ordered, sizeof(int));
                        read(sockfd, &instock, sizeof(int));
                        read(sockfd, &price, sizeof(int));
                        printf("Product id- %d\n", c.products[j].id);
                        printf("Ordered - %d; In stock - %d; Price - %d\n", ordered, instock, price);
                        c.products[j].qty = instock;
                        c.products[j].price = price;
                    }
                    j++;
                }

                int total ;
                total= calculateTotal(c);
                
                printf("Total in your cart\n");
                printf("%d\n", total);
                int payment;

                while (1){
                    printf("Please enter the amount to pay\n");
                    scanf("%d", &payment);

                    if (payment != total){
                        printf("Wrong total entered, enter again\n");
                    }else{
                        break;
                    }
                }

                char ch ;
                ch= 'y';
                printf("Payment recorded, order placed\n");
                write(sockfd, &ch, sizeof(char));
                read(sockfd, &ch, sizeof(char));
                generateReceipt(total, c, sockfd);
            }

            else if (ch == 'g'){
                char conf;
                printf("Press y/n if you want to continue\n");
                scanf("%c", &conf);
                scanf("%c", &conf);

                write(sockfd, &conf, sizeof(char));
                if (conf == 'y'){

                    int id;
                    read(sockfd, &id, sizeof(int));
                    printf("Your new customer id : %d\n", id);
                    
                }else{
                    printf("Request aborted\n");
                }
            }
            else{
                printf("Invalid choice, try again\n");
            }

            
        }
    }
    else if (user == 2){
        
        while (1){
            displayMenuAdmin();
            char ch;
            scanf("%c",&ch);
            scanf("%c",&ch);
            write(sockfd, &ch, sizeof(ch));
            

            if (ch == 'a'){
                //add a product
                int id, qty, price;
                char name[50];

                printf("Enter product name\n");
                scanf("%s", name);
                id = prodIdTaker();
                qty = quantityTaker();
                price = priceTaker();
                
                struct product p;
                p.id = id;
                strcpy(p.name, name);
                p.qty = qty;
                p.price = price;

                int n1 = write(sockfd, &p, sizeof(struct product));

                char response[80];
                int n = read(sockfd, response, sizeof(response));
                response[n] = '\0';

                printf("%s", response);
            }

            else if (ch == 'b'){
                // printf("Enter product id to be deleted\n");
                int id = prodIdTaker();
                
                write(sockfd, &id, sizeof(int));
                //deleting is equivalent to setting everything as -1

                char response[80];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);
            }

            else if (ch == 'c'){
                int id ;
                id= prodIdTaker();

                int price ;
                price= priceTaker();
                
                struct product p;
                p.id = id;
                p.price = price;
                write(sockfd, &p, sizeof(struct product));

                char response[80];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);
            }

            else if (ch == 'd'){
                int id ;
                id= prodIdTaker();
                int qty ;
                qty= quantityTaker();

                struct product p;
                p.id = id;
                p.qty = qty;
                write(sockfd, &p, sizeof(struct product));

                char response[80];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);

            }

            else if (ch == 'e'){
                getInventory(sockfd);
            }

            else if (ch == 'f'){
                break;
            }

            else{
                printf("Choice invalid, please try again\n");
            }
        }
    }

    printf("Exiting the program\n");
    close(sockfd);
    return 0;
}