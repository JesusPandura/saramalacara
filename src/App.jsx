import CamViewer from "./components/CamViewer";

function App() {
  return (
    <div style={{ padding: "20px", maxWidth: "800px", margin: "0 auto" }}>
      <h1 style={{ textAlign: "center" }}>ESP32-CAM Remote Viewer</h1>
      <CamViewer />
    </div>
  );
}

export default App;
