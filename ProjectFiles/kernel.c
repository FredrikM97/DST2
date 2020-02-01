#include "kernel.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <limits.h>

void idle(void);
list* create_List(void);
listobj* create_Listobj(TCB* task);
msg* create_msg(void);
void insert(list* mylist, listobj* pObj);
listobj* extract(listobj * pObj);
void RunningContext(void);
char* create_data(void* data, uint size_t);
msg *msg_extractObj(mailbox *mBox, msg *specific); 
exception msg_insertObj(mailbox *mBox, msg *pOb);
msg *msg_extractObj(mailbox *mBox, msg *specific);
TCB* create_TCB(uint deadline, void(*task_body)());

void deleteList(list* obj);
void deleteListobj(listobj* obj);
void deleteMailbox(mailbox* mBox);
void deleteMessage(msg* message);
void deleteTCB(TCB* TaskContext);
void deleteData(char *data);

/******************************************************************************\
                                   Globals
\******************************************************************************/

uint tickCounter;
TCB* Running;

struct threeLists{
	list* waiting;
	list* ready;
	list* timer;
}List;

struct Flags{
	char startUpMode:1;
}flag;

void tail(void){}
void head(void){}
//void isr_off(){}
//void isr_on(){}
/******************************************************************************\
                                  KernelAPI
\******************************************************************************/

//Task administration
exception init_kernel(){
	//This function initializes the kernel and its data
	//structures and leaves the kernel in start-up mode. The
	//init_kernel call must be made before any other call is
	//made to the kernel.
	//Return parameter
	//Int: Description of the functions status, i.e. FAIL/OK.
	
	//Function
	flag.startUpMode = TRUE; //Set the kernel in start up mode
	set_ticks(0); //Set tick counter to zero
	List.ready = create_List();//Create necessary data structures
	if(!List.ready) return FAIL; // IF NULL THEN FAIL
	List.timer = create_List();	
	if(!List.timer) return FAIL;
	List.waiting = create_List();
	if(!List.waiting) return FAIL;
	if(!create_task(idle, UINT_MAX)) return FAIL; //Create an idle task
	return OK; //Return status
}

exception create_task(void(* task_body)(), uint deadline){
	//This function creates a task. If the call is made in startup
	//mode, i.e. the kernel is not running, only the
	//necessary data structures will be created. However, if
	//the call is made in running mode, it will lead to a
	//rescheduling and possibly a context switch.
	//Argument
	//*task_body: A pointer to the C function holding the code
	//of the task.
	//deadline: The kernel will try to schedule the task so it
	//will meet this deadline.
	//Return parameter
	//Description of the function?s status, i.e. FAIL/OK.
	
	//Function
	TCB* thisTCB = (TCB*)calloc(1, sizeof(TCB)); //Allocate memory for TCB
	if(!thisTCB) return FAIL;
	thisTCB->DeadLine = deadline; //Set deadline in TCB
	thisTCB->PC = task_body; //Set the TCBs PC to point to the task body
	thisTCB->SP = &(thisTCB->StackSeg[STACK_SIZE-1]); //Set TCBs SP to point to the stack segment
	
	if(flag.startUpMode){ //IF start-up mode THEN
		insert(List.ready,create_Listobj(thisTCB)); //Insert new task in Readylist
		return OK; //Return status
	}else {//ELSE
		volatile uint firstExecution = TRUE;
		isr_off(); //Disable interrupts
		SaveContext(); //Save context
		if(firstExecution){//IF first execution THEN
			firstExecution = FALSE; //Set: not first execution any more
			insert(List.ready,create_Listobj(thisTCB)); //Insert new task in Readylist
			RunningContext(); //Load context
		} //ENDIF
	}//ENDIF
	return OK; //Return status
}

void run(void){
	//This function starts the kernel and thus the system of
	//created tasks. Since the call will start the kernel it will
	//leave control to the task with tightest deadline.
	//Therefore, it must be placed last in the application
	//initialization code. After this call the system will be in
	//running mode.
	
	//Function
	timer0_start(); //Initialize interrupt timer
	flag.startUpMode = FALSE; //Set the kernel in running mode
	isr_on(); //Enable interrupts
	RunningContext(); //Load context
}

void terminate(void){
	//This call will terminate the running task. All data
	//structures for the task will be removed. Thereafter,
	//another task will be scheduled for execution.
	
	//Function
	if(List.ready->pHead->pNext->pTask->DeadLine != UINT_MAX){
		deleteListobj(extract(List.ready->pHead->pNext)); //Remove running task from Readylist
		RunningContext();//Set next task to be the running task
		//and //Load context
	}
}

//Inter-Process Communication
mailbox* create_mailbox(uint nMessages, uint nDataSize){
	//This call will create a mailbox. The mailbox is a FIFO
	//communication structure used for asynchronous and
	//synchronous communication between tasks.
	//Argument
	//nof_msg: Maximum number of Messages the mailbox can hold.
	//Size_of msg: The size of one Message in the mailbox.
	//Return parameter
	//mailbox*: a pointer to the created mailbox or NULL.
	
	//Function
	mailbox* mBox = (mailbox*)calloc(1,sizeof(mailbox)); //Allocate memory for the mailbox
	if(!mBox) return NULL;
	mBox->pHead = create_msg(); //Initialize mailbox structure
	if(!mBox->pHead) {
		deleteMailbox(mBox);//free(mBox);
		return NULL;
	}
	mBox ->pTail = create_msg();
	if(!mBox->pTail){ 
		deleteMailbox(mBox);//free(mBox);
		//deleteMessage(mBox->pHead);
		return NULL;
	}
	
	mBox->nMaxMessages = nMessages; 
	mBox->nDataSize = nDataSize;
	
	mBox->pHead->pPrevious = mBox->pHead;
	mBox->pHead->pNext = mBox->pTail;
	mBox->pTail->pPrevious = mBox->pHead;
	mBox->pTail->pNext = mBox->pTail;
	
	return mBox; //Return mailbox*
}

int no_messages(mailbox *mBox){
	//This call will remove the mailbox if it is empty and return
	//OK. Otherwise no action is taken and the call will return
	//NOT_EMPTY.
	//Argument
	//mailbox*: A pointer to the mailbox to be removed.
	//Return parameter
	//OK: The mailbox was removed
	//NOT_EMPTY: The mailbox was not removed because
	//it was not empty.
	
	//Function
	if(!mBox->nMessages && mBox->pHead->pNext == mBox->pTail){ //IF mailbox is empty THEN
		deleteMailbox(mBox); //Free the memory for the mailbox
		return OK; //Return OK
	}else{ //ELSE
		return NOT_EMPTY; //Return NOT_EMPTY
	}//ENDIF
}

exception send_wait( mailbox* mBox, void* pData){
	//This call will send a Message to the specified mailbox.
	//If there is a receiving task waiting for a Message on the
	//specified mailbox, send_wait will deliver it and the
	//receiving task will be moved to the Readylist.
	//Otherwise, if there is not a receiving task waiting for a
	//Message on the specified mailbox, the sending task will
	//be blocked. In both cases (blocked or not blocked) a
	//new task schedule is done and possibly a context
	//switch. During the blocking period of the task its
	//deadline might be reached. At that point in time the
	//blocked task will be resumed with the exception:§
	//DEADLINE_REACHED. Note: send_wait  and
	//send_no_wait Messages shall not be mixed  in  the
	//same mailbox.
	//Argument
	//*mBox a pointer to the specified mailbox.
	//*Data: a pointer to a memory area where the data of
	//the communicated Message is residing.
	//Return parameter
	//exception: The exception return parameter can have
	//two possible values:
	//• OK: Normal behavior, no exception occurred.
	//• DEADLINE_REACHED: This return parameter
	//is given if the sending tasks deadline is
	//reached while it is blocked by the send_wait call.
	
	//Function
	volatile uint firstExecution = TRUE;
	isr_off(); //Disable interrupt
	SaveContext(); //Save context
	
	if(firstExecution){//IF first execution THEN
		firstExecution = FALSE; //Set: not first execution any more
		if(mBox->nBlockedMsg < 0){ //IF receiving task is waiting THEN
			msg *message;
			memcpy(mBox->pHead->pNext->pData, pData, mBox->nDataSize); //Copy senders data to the data area of the receivers Message
			message = msg_extractObj(mBox,NULL); //Remove receiving tasks Message struct from the mailbox
			insert(List.ready, extract(message->pBlock)); //Move receiving task to Readylist
			deleteMessage(message);
		}else{ //ELSE
			msg* message = create_msg(); //Allocate a Message structure
			if(!message) return FAIL;
			
			message->pData = create_data(pData, mBox->nDataSize); //Copy Data to the Message
			if(!message->pData){										
				deleteMessage(message);
				return FAIL;
			}
			//message->pData = pData; //Set data pointer
			message->Status = 2;
			msg_insertObj(mBox, message); //Add Message to the mailbox
			insert(List.waiting, extract(List.ready->pHead->pNext)); //Move sending task from Readylist to Waitinglist
		}//ENDIF
		RunningContext(); //Load context
	}else{ //ELSE
		if(Running->DeadLine <= tickCounter){ //IF deadline is reached THEN
			isr_off(); //Disable interrupt
				
			msg_extractObj(mBox, List.ready->pHead->pNext->pMessage); //Clean up mailbox entry
			free(List.ready->pHead->pNext->pMessage->pData);
			free(List.ready->pHead->pNext->pMessage);
			
			isr_on(); //Enable interrupt
			return DEADLINE_REACHED;//Return DEADLINE_REACHED
		}else{//ELSE
			return OK; //Return OK
		}//ENDIF
	}//ENDIF
	return OK;
}

exception receive_wait( mailbox* mBox, void* pData){
	//This call will attempt to receive a Message from the
	//specified mailbox. If there is a send_wait or a
	//send_no_wait Message waiting for a receive_wait or a
	//receive_no_wait Message on the specified mailbox,
	//receive_wait will collect it. If the Message was of
	//send_wait type the sending task will be moved to the
	//Readylist. Otherwise, if there is not a send Message (of
	//either type) waiting on the specified mailbox, the
	//receiving task will be blocked. In both cases (blocked or
	//not blocked) a new task schedule is done and possibly
	//a context switch. During the blocking period of the task
	//its deadline might be reached. At that point in time the
	//blocked task will be resumed with the exception:
	//DEADLINE_REACHED.
	//Argument
	//*mBox: a pointer to the specified mailbox.
	//*Data: a pointer to a memory area where the data of
	//the communicated Message is to be stored.
	//Return parameter
	//exception: The exception return parameter can have
	//two possible values:
	//• OK: Normal function, no exception occurred.
	//• DEADLINE_REACHED: This return parameter
	//is given if the receiving tasks? deadline is
	//reached while it is blocked by the receive_wait
	//call.
	
	//Function
	volatile uint firstExecution = TRUE;
	isr_off(); //Disable interrupts
	SaveContext(); //Save context
	if(firstExecution){ //IF first execution THEN
		firstExecution = FALSE; //Set: not first execution any more
		if(mBox->nBlockedMsg >= 0 && mBox->nMessages > 0){ //IF send Message is waiting THEN
			msg* message;
			memcpy(pData,mBox->pHead->pNext->pData, mBox->nDataSize); //Copy senders data to receiving tasks data area
			message = msg_extractObj(mBox, NULL); //Remove sending tasks Message struct from the mailbox
			if(message->pBlock != NULL){ //IF Message was of wait type THEN
				insert(List.ready, extract(message->pBlock)); // Move sending task to Ready list
			}else{ //ELSE
				deleteData(message->pData); //Free senders data area
			} //ENDIF
			//deleteData(message->pData); //F edited
			deleteMessage(message);
		}else{ //ELSE
			msg* message = create_msg(); //Allocate a Message structure
			if(!message) return FAIL;
			
			message->pData = pData;
			message->Status = 3;
			msg_insertObj(mBox, message); //Add Message to the mailbox
			insert(List.waiting, extract(List.ready->pHead->pNext));//Move receiving task from Readylist to Waitinglist
		} //ENDIF
		RunningContext(); //Load context
	}else{ //ELSE
		if(Running->DeadLine <= tickCounter){// IF deadline is reached THEN
			msg *message;
			isr_off(); //Disable interrupt
			
			//message = msg_extractObj(mBox, List.ready->pHead->pNext->pMessage);
			//deleteData(message->pData);
			//deleteMessage(message); //Remove send Message
			msg_extractObj(mBox, List.ready->pHead->pNext->pMessage); //Clean up mailbox entry
			free(List.ready->pHead->pNext->pMessage->pData);
			free(List.ready->pHead->pNext->pMessage);
			
			isr_on(); //Enable interrupt
			return DEADLINE_REACHED;//Return DEADLINE_REACHED
		}else{ //ELSE
			return OK; //Return OK
		} //ENDIF
	} //ENDIF
	return OK;
}

exception send_no_wait( mailbox* mBox, void* pData){
	//This call will send a Message to the specified mailbox.
	//The sending task will continue execution after the call.
	//When the mailbox is full, the oldest Message will be
	//overwritten. The send_no_wait call will imply a new
	//scheduling and possibly a context switch. Note:
	//send_wait and  send_no_wait Messages  shall not be
	//mixed in the same mailbox.
	//Argument
	//*mBox: a pointer to the specified mailbox.
	//*Data: a pointer to a memory area where the data of
	//the communicated Message is residing.
	//Return parameter
	//Description of the function?s status, i.e. FAIL/OK.
	
	//Function
	volatile uint firstExecution = TRUE;
	isr_off(); //Disable interrupts
	SaveContext(); //Save context
	if(firstExecution){ //IF first execution THEN
		firstExecution = FALSE; //Set: not first execution anymore
		if(mBox->nBlockedMsg < 0){//IF receiving task is waiting THEN
			msg* message;
			memcpy(mBox->pHead->pNext->pData, pData, mBox->nDataSize); //Copy data to receiving tasks data area.
			message = msg_extractObj(mBox,NULL); //Remove receiving tasks Message struct from the mailbox
			insert(List.ready, extract(message->pBlock)); //Move receiving task to Readylist
			deleteMessage(message);
			RunningContext(); //Load context
		}else{ //ELSE
			msg* message = create_msg();//Allocate a Message structure
			if(!message) return FAIL;
			message->pData = create_data(pData, mBox->nDataSize); //Copy Data to the Message
			if(!message->pData){										//Fun fact, skapar vi data crashar allt för nTest
				deleteMessage(message);
				return FAIL;
			}
			//message->pData = pData;
			message->Status = 4;
			if(mBox->nMaxMessages == mBox->nMessages){ //IF mailbox is full THEN
					deleteMessage(msg_extractObj(mBox, NULL)); //Remove the oldest Message struct
			} //ENDIF
			msg_insertObj(mBox, message); //Add Message to the mailbox
		} //ENDIF
	} //ENDIF
	return OK; //Return status
}

exception receive_no_wait( mailbox* mBox, void* pData){
	//This call will attempt to receive a Message from the
	//specified mailbox. The calling task will continue
	//execution after the call. When there is no send
	//Message available, or if the mailbox is empty, the
	//function will return FAIL. Otherwise, OK is returned.
	//The call might imply a new scheduling and possibly a
	//context switch.
	//Argument
	//*mBox: a pointer to the specified mailbox.
	//*Data: a pointer to the Message.
	//Return parameter
	//Integer indicating whether or not a Message was
	//received (OK/FAIL).
	
	//Function
	volatile uint firstExecution = TRUE;
	volatile exception status = FAIL;
	isr_off(); //Disable interrupts
	SaveContext(); //Save context
	if(firstExecution){ //IF first execution THEN
		firstExecution = FALSE; //Set: not first execution any more
		if(mBox->nMessages > 0 && mBox->pHead->pNext->Status != 3){ //IF send Message is waiting THEN //Borde det inte bara kolla om det finns en send  //F ändrat
			msg* message;
			memcpy(pData, mBox->pHead->pNext->pData, mBox->nDataSize); //Copy senders data to receiving tasks data area
			message = msg_extractObj(mBox, NULL); //Remove sending tasks Message struct from the mailbox
			if(message->pBlock != NULL){ //IF Message was of wait type THEN
				insert(List.ready, extract(message->pBlock));// Move sending task to Readylist
			}else{ //ELSE
				 deleteData(message->pData); //Free senders data area
			} //ENDIF
			//deleteData(message->pData); 
			deleteMessage(message);
			status = OK;
		} //ENDIF
		else{
			status = FAIL;
		}
		RunningContext(); //Load context
	} //ENDIF
	
	return status; //Return status on received Message
}
//Timing functions
exception wait(uint nTicks){
	//This call will block the calling task until the given
	//number of ticks has expired.
	//Argument
	//nTicks: the duration given in number of ticks
	//Return parameter
	//exception: The exception return parameter can have
	//two possible values:
	//• OK: Normal function, no exception occurred.
	//• DEADLINE_REACHED: This return parameter
	//is given if the receiving tasks? deadline is
	//reached while it is blocked by the receive_wait
	//call.
	
	//Function
	exception status;
	volatile uint firstExecution = TRUE;
	isr_off(); //Disable interrupt
	
	SaveContext(); //Save context
	if(firstExecution){ //IF first execution THEN
		firstExecution = FALSE; //Set: not first execution any more
		List.ready->pHead->pNext->nTCnt = nTicks + tickCounter;
		insert(List.timer, extract(List.ready->pHead->pNext)); //Place running task in the Timerlist
		RunningContext(); //Load context
	}else{ //ELSE
		if(tickCounter >= Running->DeadLine){//IF deadline is reached THEN
			status = DEADLINE_REACHED; //Status is DEADLINE_REACHED
		}else{ //ELSE
			status = OK;//Status is OK
		} //ENDIF
	} //ENDIF
	return status; //Return status
}

void set_ticks( uint nTicks){
	//This call will set the tick counter to the given value.
	//Argument
	//nTicks: the new value of the tick counter
	
	//Function
	tickCounter = nTicks; //Set the tick counter.
}

uint ticks(void){
	//This call will return the current value of the tick counter
	//Return parameter
	//A 32 bit value of the tick counter
	
	//Function
	return tickCounter; //Return the tick counter
}

uint deadline(void){
	//This call will return the deadline of the specified task
	//Return parameter
	//the deadline of the given task
	
	//Function
	return Running->DeadLine; //Return the deadline of the current task
}

void set_deadline(uint deadline){
	//This call will set the deadline for the calling task. The
	//task will be rescheduled and a context switch might
	//occur.
	//Argument
	//deadline: the new deadline given in number of ticks.
	
	//Function
	volatile uint firstExecution = TRUE;
	isr_off(); //Disable interrupt
	SaveContext(); //Save context
	if(firstExecution){ //IF first execution THEN
		firstExecution = FALSE; //Set: not first execution any more
		Running->DeadLine = deadline; //Set the deadline field in the calling TCB.
		insert(List.ready,extract(List.ready->pHead->pNext)); //Reschedule Readylist (Lazy way)
		RunningContext(); //Load context
	} //ENDIF
}

void TimerInt(void){
	//This function is not available for the user to call.
	//It is called by an ISR (Interrupt Service Routine)
	//invoked every tick. Note, context is automatically saved
	//prior to call and automatically loaded on function exit.
	
	//Function
	tickCounter++; //Increment tick counter
	//Check the Timerlist for tasks that are ready for
	//execution, move these to Readylist
	while(List.timer->pHead->pNext != List.timer->pTail && List.timer->pHead->pNext->nTCnt <= tickCounter){
			insert(List.ready, extract(List.timer->pHead->pNext));
	}
	
	//Check the Waitinglist for tasks that have expired
	//deadlines, move these to Readylist and clean up
	//their mailbox entry.
	while(List.waiting->pHead->pNext != List.waiting->pTail && List.waiting->pHead->pNext->pTask->DeadLine <= tickCounter){
		insert(List.ready, extract(List.waiting->pHead->pNext)); //List.waiting->pHead->pNext->pMessage->pData	
	}
	Running = List.ready->pHead->pNext->pTask;
	}

void RunningContext(){
	Running = List.ready->pHead->pNext->pTask;
	LoadContext(); //Load context
}

/******************************************************************************\
                                  TASKS
\******************************************************************************/

void idle(void){
	while(TRUE){
		//SaveContext();
		//TimerInt();
		//LoadContext();
	}
}

/******************************************************************************\
                                  ListAPI
\******************************************************************************/
// dlist.c
// #include "dlist.h"

list* create_List(){
	TCB* task;
	list* mylist = (list *)calloc(1, sizeof(list));
	if (!mylist) {
		return NULL;
	}
	task = create_TCB(0, head);
	if(!task) return NULL;
	mylist->pHead = create_Listobj(task);
	if (!mylist->pHead) {
		free(mylist);
		return NULL;
	}
	
	task = create_TCB(UINT_MAX, tail);
	if(!task) return NULL;
	mylist->pTail = create_Listobj(task);
	if (!mylist->pTail) {
		deleteListobj(mylist->pHead);
		free(mylist);
		return NULL;
	}
	mylist->pHead->nTCnt = UINT_MAX;
	mylist->pTail->nTCnt = UINT_MAX;

	mylist->pHead->pPrevious = mylist->pHead;
	mylist->pHead->pNext = mylist->pTail;
	mylist->pTail->pPrevious = mylist->pHead;
	mylist->pTail->pNext = mylist->pTail;
	return mylist;
}

TCB* create_TCB(uint deadline, void(*task_body)()){
	TCB* pTask = (TCB*)calloc(1, sizeof(TCB));
	if(!pTask) return NULL;
	pTask->SPSR = 0; 
	pTask->PC = task_body;
	pTask->SP = &(pTask->StackSeg[STACK_SIZE-1]);	
	pTask->DeadLine = deadline;
	return pTask;
}

listobj* create_Listobj(TCB* task){
	listobj* myobj = (listobj *)calloc(1, sizeof(listobj));
	if (!myobj){
		return NULL;
	}
	myobj->pTask = task;
	//myobj->nTCnt = tickCounter;
	return (myobj);
}

msg* create_msg(){
	msg* message = (msg*)calloc(1 ,sizeof(msg));
	if(!message) return NULL;
	return message;
}

char* create_data(void* data, uint size_t){
	char* obj = (char*)calloc(1 ,size_t);
	if(!obj) return NULL;
	if(data) memcpy(obj, data, size_t);
	return obj;
}

void insert(list* mylist, listobj* pObj){
	if(pObj){ // if there's an object
		// insert first in list or...
		listobj* pMarker;
		pMarker = mylist->pHead;
		
		if(mylist == List.ready || mylist == List.waiting){ //sort on Deadline
			while(pMarker != List.ready->pTail && pMarker != List.waiting->pTail && pMarker->pNext->pTask->DeadLine < pObj->pTask->DeadLine)
				pMarker = pMarker->pNext;
		}else if(mylist == List.timer){ //sort on nTCnt
			while(pMarker->pNext->nTCnt < pObj->nTCnt)
				pMarker = pMarker->pNext;
		}
		
		/* Position found, insert element */
		pObj->pNext = pMarker->pNext;
		pObj->pPrevious = pMarker;
		pMarker->pNext = pObj;
		pObj->pNext->pPrevious = pObj;
	}
}

listobj* extract(listobj* pObj){
	if(pObj->pPrevious == pObj || pObj->pNext == pObj) // No picking head/tail
		return NULL;
	pObj->pPrevious->pNext = pObj->pNext;
	pObj->pNext->pPrevious = pObj->pPrevious;
	pObj->pNext = pObj->pPrevious = NULL;
	
	return pObj;
}

exception msg_insertObj(mailbox *mBox, msg *pObj){ 
	if(mBox->nMaxMessages == mBox->nMessages) //IF mailbox is full THEN
		deleteMessage(msg_extractObj(mBox, NULL)); //Remove the oldest Message struct
	
	if(pObj->Status != 4){ //F ändrat
		pObj->pBlock = List.ready->pHead->pNext; 
		List.ready->pHead->pNext->pMessage = pObj; 
	}
	pObj->pNext = mBox->pTail;
	pObj->pPrevious = mBox->pTail->pPrevious;
	mBox->pTail->pPrevious = pObj;
	pObj->pPrevious->pNext = pObj;
	
	switch(pObj->Status){
		case 2:
			mBox->nMessages++;
			mBox->nBlockedMsg++;
			break;
		case 3: 
			mBox->nMessages++;
			mBox->nBlockedMsg--;
			break;
		case 4:
				mBox->nMessages++;
			break;
		case 5:
			if(mBox->nMessages > 0){
				mBox->nMessages--;
			}
			break;
		default:
			return FAIL;
			break;
		}
	return OK;
}

msg *msg_extractObj(mailbox *mBox, msg *specific){ 
	msg *temp;
	temp = mBox->pHead->pNext;
	
	if(mBox->nMessages != 0 || mBox->nBlockedMsg != 0){
		if(specific != NULL){ 
			temp = specific->pBlock->pMessage;
		}
		temp->pPrevious->pNext = temp->pNext;
		temp->pNext->pPrevious =  temp->pPrevious;
		temp->pNext = temp->pPrevious = NULL;
		
		switch(temp->Status){
		case 2:
			mBox->nMessages--;
			mBox->nBlockedMsg--;
			break;
		case 3: 
			mBox->nMessages--;
			mBox->nBlockedMsg++;
			break;
		case 4: 
				mBox->nMessages--;
				break;
		case 5: 
			if(mBox->nMessages > 0){
				mBox->nMessages--;
			}
			break;
		default:
			return FAIL;
			break;
				
		}
		return temp;
	}else{
		return FAIL; 
	}
}

/******************************************************************************\
                                 deconstructors
\******************************************************************************/

void deleteList(list* obj){
	deleteListobj(obj->pHead);
	deleteListobj(obj->pTail);
	free(obj);
}

void deleteListobj(listobj* obj){
	deleteTCB(obj->pTask);
	free(obj);
}

void deleteMailbox(mailbox* mBox){
	deleteMessage(mBox->pHead);
	deleteMessage(mBox->pTail);
	free(mBox);
}

void deleteMessage(msg* message){
	//free(message->pData);
	free(message);
}
void deleteData(char *data){
	free(data);
}

void deleteTCB(TCB* TaskContext){
	free(TaskContext);
}
