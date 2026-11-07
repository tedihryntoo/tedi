
const addressInput = document.getElementById("deviceAddress");
const message = document.getElementById("message");

function normalizeAddress(value) {
  let address = value.trim();

  if (!address) return "";

  if (!/^https?:\/\//i.test(address)) {
    address = "http://" + address;
  }

  return address.replace(/\/+$/, "");
}

document.getElementById("openPanel").addEventListener("click", () => {
  const address = normalizeAddress(addressInput.value);

  if (!address) {
    message.textContent = "Masukkan alamat IP ESP32.";
    return;
  }

  try {
    const parsed = new URL(address);

    if (!["http:", "https:"].includes(parsed.protocol)) {
      throw new Error("Alamat tidak valid");
    }

    message.textContent = "Membuka panel ESP32...";
    window.location.href = address + "/";

  } catch (error) {
    message.textContent =
      "Alamat tidak valid. Contoh: http://192.168.4.1";
  }
});

addressInput.addEventListener("keydown", event => {
  if (event.key === "Enter") {
    document.getElementById("openPanel").click();
  }
});
