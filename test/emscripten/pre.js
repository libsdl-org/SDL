const searchParams = new URLSearchParams(window.location.search);

Module.preRun = () => {
};

const arguments = [];
for (let i = 1; true; i++) {
  const arg_i = searchParams.get(`arg_${i}`);
  if (arg_i == null) {
    break;
  }
  arguments.push(arg_i);
}

Module.arguments = arguments;

if (searchParams.get("loghtml") === "1") {
  const divTerm = document.createElement("div");
  divTerm.id = "terminal";
  document.body.append(divTerm);

  function printToStdOut(msg, id) {
    const divMsg = document.createElement("div", {class: "stdout"});
    divMsg.id = id;
    divMsg.append(document.createTextNode(msg));
    divTerm.append(divMsg);
    return divMsg;
  }

  Module.print = (msg) => {
    console.log(msg);
    printToStdOut(msg, "stdout");
  }

  Module.printErr = (msg) => {
    console.error(msg);
    const e = printToStdOut(msg, "stderr");
    e.style = "color:red";
  }

  const divQuit = document.createElement("div");
  divQuit.id = "process-quit";
  document.body.append(divQuit);

  Module.quit = (msg) => {
    divQuit.innerText = msg;
    console.log(`QUIT: ${msg}`)
  }

  Module.onabort = (msg) => {
    printToStdOut(`ABORT: ${msg}`, "stderr");
    console.log(`ABORT: ${msg}`);
  }
}
