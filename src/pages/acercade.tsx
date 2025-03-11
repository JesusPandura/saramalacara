import React from "react";

const AcercaDe: React.FC = () => {
  return (
    <div className="max-w-4xl mx-auto p-6 bg-gray-800 text-white rounded-lg shadow-lg mt-28">
      <h1 className="text-3xl font-bold mb-4 text-center">
        Acerca de Nosotros
      </h1>
      <p className="mb-4 text-justify">
        Bienvenido a nuestra plataforma de cursos de tarot. Aquí, te ofrecemos
        una variedad de cursos diseñados para ayudarte a explorar y dominar el
        arte del tarot.
      </p>
      <h2 className="text-2xl font-semibold mt-6 mb-2">Nuestra Misión</h2>
      <p className="mb-4 text-justify">
        Nuestra misión es proporcionar educación accesible y de calidad sobre el
        tarot, fomentando el crecimiento personal y la autocomprensión a través
        de esta práctica ancestral.
      </p>
      <h2 className="text-2xl font-semibold mt-6 mb-2">
        Características de Nuestros Cursos
      </h2>
      <ul className="list-disc list-inside mb-4">
        <li>Instrucción de expertos en tarot.</li>
        <li>Acceso a materiales de estudio exclusivos.</li>
        <li>Comunidad de apoyo y aprendizaje.</li>
        <li>Certificación al finalizar los cursos.</li>
      </ul>
      <h2 className="text-2xl font-semibold mt-6 mb-2">Contáctanos</h2>
      <p>
        Si tienes preguntas o necesitas más información, no dudes en{" "}
        <a href="/contacto" className="text-blue-400 underline">
          contactarnos
        </a>
        .
      </p>
    </div>
  );
};

export default AcercaDe;
