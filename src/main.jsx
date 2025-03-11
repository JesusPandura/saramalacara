import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";

// Añadimos algunos estilos globales
const style = document.createElement("style");
style.textContent = `
  body {
    margin: 0;
    font-family: Arial, sans-serif;
    background-color: #f0f0f0;
  }
`;
document.head.appendChild(style);

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
