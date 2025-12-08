using UnityEngine;

namespace Game
{
    public class MovingRewardItemView : MonoBehaviour
    {
        [SerializeField] private RectTransform rectTransform;

        // Eliptik yörünge için:
        [SerializeField] private float ellipseFactor = 0.6f; // Yarıçapın Y’de ne kadar “basık” olacağını belirler.

        private bool _isRotating;
        private float _orbitRadius;          // X ekseni yarıçapı
        public float orbitSpeed = 30f;       // derece/saniye

        private float _currentAngle = 0f;

        // Ön/arka izlenimi için referanslar
        private RectTransform _parentRectTransform;
        private RectTransform _starRectTransform;

        public void Update()
        {
            if (!_isRotating)
                return;

            _currentAngle += orbitSpeed * Time.deltaTime;
            float angleInRadians = _currentAngle * Mathf.Deg2Rad;

            // Eliptik yörünge
            float x = Mathf.Cos(angleInRadians) * _orbitRadius;
            float y = Mathf.Sin(angleInRadians) * _orbitRadius * ellipseFactor;

            // UI için anchoredPosition kullanmak daha temiz
            rectTransform.anchoredPosition = new Vector2(x, y);

            UpdateFrontBack(angleInRadians);
        }

        /// <summary>
        /// Sinüsün işaretine göre kristali bazen yıldızın önüne, bazen arkasına al.
        /// Ayrıca küçük bir scale değişimi ile pseudo-3D derinlik veriyoruz.
        /// </summary>
        private void UpdateFrontBack(float angleInRadians)
        {
            if (_parentRectTransform == null || _starRectTransform == null)
                return;

            bool isFront = Mathf.Sin(angleInRadians) < 0f; // aşağıdayken önde, yukarıdayken arkada gibi düşünebiliriz.

            if (isFront)
            {
                // ÖNDE: biraz büyüt + en öne al
                rectTransform.localScale = new Vector3(1.1f, 1.1f, 1f);
                rectTransform.SetAsLastSibling();  // Aynı parent altındaki son child en üstte görünür
            }
            else
            {
                // ARKADA: biraz küçült + yıldızın arkasına al
                rectTransform.localScale = new Vector3(0.9f, 0.9f, 1f);

                // Yıldızın siblingIndex'ini bulup onun altına yerleştiriyoruz ki yıldız üstte kalsın.
                int starIndex = _starRectTransform.GetSiblingIndex();
                int behindIndex = Mathf.Max(0, starIndex - 1);
                rectTransform.SetSiblingIndex(behindIndex);
            }
        }

        public void SetIsRotating(bool isRotating)
        {
            _isRotating = isRotating;
        }

        public void SetOrbitRadius(float orbitRadius)
        {
            _orbitRadius = orbitRadius;
        }

        /// <summary>
        /// Parent (yıldız container) ve yıldız rect’ini alarak
        /// başlangıç sibling düzenini ayarlar.
        /// </summary>
        public void Init(RectTransform parentRect, RectTransform starRect)
        {
            _parentRectTransform = parentRect;
            _starRectTransform = starRect;

            rectTransform.SetParent(parentRect);
            rectTransform.localScale = Vector3.one;
            rectTransform.anchoredPosition = Vector2.zero;

            // Başlangıçta yıldızın üstünde dursun:
            if (_starRectTransform != null)
            {
                _starRectTransform.SetSiblingIndex(0);
                rectTransform.SetSiblingIndex(1);
            }
        }

        public void SetSize(Vector2 size)
        {
            rectTransform.sizeDelta = size;
        }

        public void SetStatus(bool status)
        {
            gameObject.SetActive(status);
        }

        public RectTransform GetRectTransform()
        {
            return rectTransform;
        }

        public void DestroyObject()
        {
            Destroy(gameObject);
        }
    }
}
