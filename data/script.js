
$(document).ready(function(){
	
	initWebSocket();
	
	$("#btlight").click(function() {
		websocket.send('toggle');
	});
});

var gateway = `ws://${window.location.hostname}/ws`;

var websocket;

function initWebSocket() {
	console.log('Trying to open a WebSocket connection...');
	websocket = new WebSocket(gateway);
	websocket.onopen    = onOpen;
	websocket.onclose   = onClose;
	websocket.onmessage = onMessage; // <-- add this line
}	 

function getDoorsState(){
	websocket.send("getDoorsState");
	}

function onOpen(event) {
	console.log('Connection opened');
	getDoorsState();
	}

function onClose(event) {
	console.log('Connection closed');
	setTimeout(initWebSocket, 2000);
	}

function toggle(){
	websocket.send('toggle');
	}

function onRefresh(){}

function onMessage(event) {
    console.log(event.data);
	var myObj = event.data;
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

	console.log(myObj);
	
	if(myObj.DoorMathState == "Closed"){
		$("#DoorMathState").attr("src", "DoorClosed.png");
	}else{
		$("#DoorMathState").attr("src", "DoorOpen.png");
	}
	
	if(myObj.DoorCaroState == "Closed"){
		$("#DoorCaroState").attr("src", "DoorClosed.png");
	}else{
		$("#DoorCaroState").attr("src", "DoorOpen.png");
	}
}