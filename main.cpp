#include <iostream>
#include <pthread.h>
#include <math.h>
#include <string>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
struct node { //struct to hold our info. for the main thread to easily access
    node* next;
    char symbol;
    int id; //to track what nodes to print in what order
    string code;
    double probability;
    double cumulative_sum;
    double count; //conuts freq. of letters
    pthread_mutex_t bsem;
    sem_t *sem; //semaphore to signal done copying to main thread
    sem_t *delay; //semaphore to signal done printing so next thread can print
    int turn;
    pthread_cond_t waitTurn = PTHREAD_COND_INITIALIZER;
};
int findFreq(string in, node*& head) { //finding frequences of symbols and storing them into the struct.
    //first count how many of each unique item there are
    int totalCharLength = int(in.length());
    int size = 0;
    for (int i = 0; i < totalCharLength; i++) {
        if (head == NULL) { //adding first element into list
            node* tmp = new node();
            tmp->symbol = in[i];
            tmp->next = NULL;
            tmp->count = 1;
            head = tmp;
            continue;
        }
        node* curr = head;
        node* prev = NULL;
        bool exists = false;
        while (curr != NULL) { //checking if element already exists in list so can update it's count
            prev = curr;
            if (curr->symbol == in[i]) { //element exists already, update it's count
                curr->count += 1;
                exists = true;
                break;
            }
            curr = curr->next;
        }
        if (exists)
            continue;
        node* tmp = new node(); //symbol doesn't exist in list so create new symbol
        tmp->symbol = in[i];
        tmp->count = 1;
        tmp->next = NULL;
        prev->next = tmp;
    }
    //Now can find the frequencies of each unique symbol that we counted above
    node* curr = head;
    while (curr != NULL) {
        curr->probability = curr->count / totalCharLength;
        size++;
        curr = curr->next;
    }
    return size;
}
node* copyToOneAddr(node*& curr, node*& singleNode, int i) { //copying values from current node to a node that will always have the same address called singleNode
    singleNode->symbol = curr->symbol;
    singleNode->probability = curr->probability;
    singleNode->count = curr->count;
    singleNode->id = i;
    //cout << singleNode->turn << endl;
    //singleNode->cumulative_sum += curr->probability;
    return singleNode;
}
int find_length_of_code(double px, int length) { //finding length of our code to see what is the length required for the final code
    length = ceil(log2(1 / px) + 1);
    return length;
}
string fbarBinary(double fbar_sum, int length) { //function to generate the binary of the fbar value by converting from decimal to binary and returning the final generated code
    string binaryCode = "";
    double decimal = fbar_sum;
    for (int l = 0; l < length; l++) { //add binary digits up to the max length for that probability
        decimal *= 2;
        if (decimal >= 1) { //adding binary value to our code
            binaryCode += "1";
        }
        else {
            binaryCode += "0";
        }
        string binaryStr = to_string(decimal);
        for (int i = 0; i < binaryStr.length(); i++) { //getting only the values after the decimal point to calculate for the next binary value
            if (binaryStr[i] == '.') {
                binaryStr = binaryStr.substr(i, binaryStr.length() - 1);
            }
        }
        decimal = stod(binaryStr); //converting binary string to decimal so can solve for the next value for the code
    }
    return binaryCode;
}
string fbar(double prev_sum, double half_px) { //function that calculates the fbar value and then calls a function to find the length of our code and then calls a second function to convert from decimal to binary. This function then receives the binary code and then returns that binary code to the calling function
    double fbar_sum = prev_sum + half_px;
    int length = 0;
    length = find_length_of_code(half_px * 2, length);
    string code = fbarBinary(fbar_sum, length);
    return code;
}
void* shannon_fano_elias(void* current) { //function to calculate the shannon-fano-elias algorithm, this function will call other helper functions to do their part and then store the final binary code in the struct. The child thread will print the code
    node* curr = (node*)current;
    
    pthread_mutex_lock(&curr->bsem);
    int id = curr->id;
    char symbol = curr->symbol;
    double probability = curr->probability;
    double cumulative_sum = curr->cumulative_sum += curr->probability; //caclulating cumulative sum in the thread
    //pthread_mutex_unlock(&temp->bsem);
    sem_post(curr->sem); //let main thread know we finished copying values so it can create next thread
    //pthread_mutex_lock(&curr->bsem);
    while (id != curr->turn) {
        pthread_cond_wait(&curr->waitTurn, &curr->bsem);
    }
    pthread_mutex_unlock(&curr->bsem);
    //sem_post(curr->sem);//signal to main done copying, child thread continues its execution
    //curr->cumulative_sum += curr->probability; //caclulating cumulative sum in the thread
    double half_px = /*curr->*/probability / 2;
    double prev_cumu_sum = /*curr->*/cumulative_sum - /*curr->*/probability; //getting previous value of cumulative sum to pass to fbar
    string code;
    /*curr->*/code = fbar(prev_cumu_sum, half_px);
    //sem_wait(curr->delay);
    pthread_mutex_lock(&curr->bsem);
    //cout << curr->id << " " << curr->turn << endl;
    cout << "Symbol " << /*curr->*/symbol << ", Code: " << /*curr->*/code << endl;
    //sem_post(curr->delay);
    curr->turn++;
    pthread_cond_broadcast(&curr->waitTurn);
    pthread_mutex_unlock(&curr->bsem);
    //curr->turn++;
    //pthread_mutex_unlock(&curr->bsem);
    return nullptr;
}
int main() {
    int initial_value = 0; //initial value for the semaphore
    int delay_value = 1;
    char delayName[] = "NAME";
    char initName[] = "UNIQUE";
    node* head = NULL;
    string in;
    getline(cin, in); //getting input of one line of characters
    int size = findFreq(in, head); //gets size of our list of elements
    pthread_t tid[size]; //creating n threads below
    node* curr = head;
    cout << "SHANNON-FANO-ELIAS Codes:\n" << endl;
    node* singleNode = new node(); //create one node and pass only this to the child thread so always have the same address when passing to the child thread
    singleNode->turn = 0; //to control what child will print
    sem_unlink(initName); //making sure to delete semaphore if it was not deleted properly before creating it again
    sem_unlink(delayName);
    singleNode->sem = sem_open(initName, O_CREAT, 0600, initial_value);
    singleNode->delay = sem_open(delayName, O_CREAT, 0600, delay_value);
    for (int i = 0; i < size; i++) {
        //sem_wait(singleNode->delay); //delay copying until previous thread finished printing
        singleNode = copyToOneAddr(curr, singleNode, i);
        if (pthread_create(&tid[i], nullptr, shannon_fano_elias, singleNode) != 0) { //executing shanon-fano-elias algo. using posix threads to calculate for each code
            cout << "Can not create the thread" << endl;
            return -1;
        }
        sem_wait(singleNode->sem); //waiting until child thread done copying value to local variable in thread
        curr = curr->next;
    }
    sem_unlink(initName);
    sem_unlink(delayName); //making sure to delete semaphore
    for (int i = 0; i < size; i++) {
        pthread_join(tid[i], nullptr); //waiting for threads to finish execution
    }
    delete singleNode;
    delete head; //deleting the linked list
    return 0;
}
